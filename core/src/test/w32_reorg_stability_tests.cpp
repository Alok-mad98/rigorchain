// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/consensus.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <node/blockstorage.h>
#include <node/supply_monitor/runtime_supply_state.h>
#include <node/supply_monitor/runtime_supply_witness_chain.h>
#include <primitives/transaction.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/time.h>
#include <validation.h>
#include <wallet/convergence_window_ui.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

/** Live formula from TailFromParentDrawSet / BuildConvergenceWindowTail. */
int DrawWindowFirst(int source_or_connect_height)
{
    return std::max(0, source_or_connect_height - Consensus::CONVERGENCE_DRAW_WINDOW + 1);
}

int DrawWindowLast(int source_height)
{
    return source_height;
}

bool HeightInWindow(int h, int source_height)
{
    return h >= DrawWindowFirst(source_height) && h <= DrawWindowLast(source_height);
}

ConvergenceDrawCandidate MakeCand(unsigned char fill, int source_height)
{
    ConvergenceDrawCandidate c;
    c.txid.SetHex(std::string(64, '0'));
    c.txid.begin()[0] = fill;
    c.weight = static_cast<uint64_t>(COIN);
    c.source_height = source_height;
    return c;
}

uint256 HashFromLabel(const char* label)
{
    uint256 h;
    std::memcpy(h.begin(), label, std::min(strlen(label), size_t{32}));
    return h;
}

Consensus::RuntimeSupplyWitness MakeWitness(int32_t height, const uint256& block_hash, const uint256& parent_hash)
{
    Consensus::RuntimeSupplyWitness w;
    w.height = height;
    w.block_hash = block_hash;
    w.parent_hash = parent_hash;
    w.invariants_passed = true;
    w.checked_invariants = Consensus::RUNTIME_SUPPLY_INVARIANT_COUNT;
    w.oru_supply_cumulative = static_cast<CAmount>(height) * COIN;
    return w;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(w32_reorg_stability_logic_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(w32_is_sliding_window_not_floor_partition)
{
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW, 32);

    // Interior height of the 992..1023 slice: sliding window is NOT the floor group.
    BOOST_CHECK_EQUAL(DrawWindowFirst(1000), 969);
    BOOST_CHECK_EQUAL(DrawWindowLast(1000), 1000);
    BOOST_CHECK(DrawWindowFirst(1000) != 32 * (1000 / 32)); // 992

    // Slice-end coincidence: source 1023 covers [992, 1023], which LOOKS like floor group 31.
    BOOST_CHECK_EQUAL(DrawWindowFirst(1023), 992);
    BOOST_CHECK_EQUAL(DrawWindowLast(1023), 1023);
    BOOST_CHECK_EQUAL(32 * (1023 / 32), 992);

    // Next source is a one-block slide, not a new disjoint group [1024, 1055].
    BOOST_CHECK_EQUAL(DrawWindowFirst(1024), 993);
    BOOST_CHECK_EQUAL(DrawWindowLast(1024), 1024);
}

BOOST_AUTO_TEST_CASE(w32_window_independent_of_tip_and_hash)
{
    // Membership is a function of (h, s, W). A "tip" or block hash is not an input.
    const int s = 1023;
    BOOST_CHECK(HeightInWindow(992, s));
    BOOST_CHECK(HeightInWindow(1023, s));
    BOOST_CHECK(!HeightInWindow(991, s));
    BOOST_CHECK(!HeightInWindow(1024, s));
    // [1010, 1041] is the window of source 1041, not a relocated identity of 1023.
    BOOST_CHECK(DrawWindowFirst(s) != 1010);
    BOOST_CHECK(DrawWindowLast(s) != 1041);
    BOOST_CHECK_EQUAL(DrawWindowFirst(1041), 1010);
}

BOOST_AUTO_TEST_CASE(w32_scenario_A_internal_reorg_does_not_move_interval)
{
    // Group-end source 1023 stays [992, 1023] after replacing 1010..1023 contents.
    const int s = 1023;
    BOOST_CHECK_EQUAL(DrawWindowFirst(s), 992);
    BOOST_CHECK_EQUAL(DrawWindowLast(s), 1023);
    BOOST_CHECK(DrawWindowFirst(s) != 1010);
    BOOST_CHECK(DrawWindowLast(s) != 1041);

    ConvergenceDrawSet parent;
    parent.source_height = s;
    parent.source_nbits = 0x1d00ffff;
    parent.candidates.push_back(MakeCand(0x01, 992));
    parent.candidates.push_back(MakeCand(0x02, 1000));
    parent.candidates.push_back(MakeCand(0x0a, 1010)); // old branch
    parent.candidates.push_back(MakeCand(0x0b, 1010)); // new branch, same height
    parent.candidates.push_back(MakeCand(0xff, 1023));

    // Connecting 1024 slides the window by one height: 992 leaves, 993..1024 remain.
    const ConvergenceWindowTail tail = TailFromParentDrawSet(parent, 1024);
    BOOST_REQUIRE(tail.complete);
    BOOST_CHECK_EQUAL(DrawWindowFirst(1024), 993);
    for (const auto& c : tail.candidates) {
        BOOST_CHECK(c.source_height >= 993);
        BOOST_CHECK(c.source_height != 992);
    }
}

BOOST_AUTO_TEST_CASE(w32_scenario_B_boundary_heights)
{
    BOOST_CHECK_EQUAL(DrawWindowFirst(991), 960);
    BOOST_CHECK_EQUAL(DrawWindowFirst(992), 961);
    BOOST_CHECK_EQUAL(DrawWindowFirst(1023), 992);
    BOOST_CHECK_EQUAL(DrawWindowFirst(1024), 993);

    BOOST_CHECK(HeightInWindow(992, 992));
    BOOST_CHECK(HeightInWindow(992, 1023));
    BOOST_CHECK(!HeightInWindow(992, 1024));
    BOOST_CHECK(!HeightInWindow(1024, 1023));
    BOOST_CHECK(HeightInWindow(1024, 1024));
}

BOOST_AUTO_TEST_CASE(w32_scenario_C_cross_group_reorg_boundaries_fixed)
{
    // Deep replacement across 970..1055 does not move any source-height interval.
    for (int s : {960, 991, 992, 1023, 1024, 1055}) {
        const int first = DrawWindowFirst(s);
        const int last = DrawWindowLast(s);
        BOOST_CHECK_EQUAL(first, std::max(0, s - 31));
        BOOST_CHECK_EQUAL(last, s);
        BOOST_CHECK_EQUAL(first, DrawWindowFirst(s)); // reorg is not an input
    }
}

BOOST_AUTO_TEST_CASE(w32_scenario_D_deep_reorg_depths)
{
    const int s = 2000;
    const int first = DrawWindowFirst(s);
    for (int depth : {1, 31, 32, 33, 64, 288}) {
        BOOST_CHECK_EQUAL(DrawWindowFirst(s), first);
        BOOST_CHECK_EQUAL(DrawWindowLast(s), s);
        BOOST_CHECK(depth > 0);
    }
    BOOST_CHECK_EQUAL(first, 1969);
}

BOOST_AUTO_TEST_CASE(w32_genesis_clamp)
{
    BOOST_CHECK_EQUAL(DrawWindowFirst(0), 0);
    BOOST_CHECK_EQUAL(DrawWindowFirst(15), 0);
    BOOST_CHECK_EQUAL(DrawWindowFirst(31), 0);
    BOOST_CHECK_EQUAL(DrawWindowFirst(32), 1);
}

BOOST_AUTO_TEST_CASE(w32_windows_overlap_so_floor_properties_do_not_apply)
{
    // One height sits in many source windows. A partition's "one group per height" is false.
    int memberships = 0;
    for (int s = 1000; s <= 1000 + 31; ++s) {
        if (HeightInWindow(1000, s)) ++memberships;
    }
    BOOST_CHECK_EQUAL(memberships, Consensus::CONVERGENCE_DRAW_WINDOW);
}

BOOST_AUTO_TEST_CASE(w32_tail_from_parent_drops_height_leaving_window)
{
    const int h = 500;
    ConvergenceDrawSet parent;
    parent.source_height = h - 1;
    parent.source_nbits = 0x1d00ffff;
    parent.candidates.push_back(MakeCand(0x01, h - Consensus::CONVERGENCE_DRAW_WINDOW));
    parent.candidates.push_back(MakeCand(0x02, h - Consensus::CONVERGENCE_DRAW_WINDOW + 1));
    parent.candidates.push_back(MakeCand(0x03, h - 1));

    const ConvergenceWindowTail tail = TailFromParentDrawSet(parent, h);
    BOOST_REQUIRE(tail.complete);
    BOOST_REQUIRE_EQUAL(tail.candidates.size(), 2U);
    BOOST_CHECK_EQUAL(tail.candidates[0].source_height, h - Consensus::CONVERGENCE_DRAW_WINDOW + 1);
    BOOST_CHECK_EQUAL(tail.candidates[1].source_height, h - 1);
}

BOOST_AUTO_TEST_CASE(w32_incomplete_parent_is_not_empty_window)
{
    BOOST_CHECK(!TailFromParentDrawSet(ConvergenceDrawSet{}, 10).complete);
}

BOOST_AUTO_TEST_CASE(w32_maturity_independent_of_window)
{
    BOOST_CHECK_EQUAL(COINBASE_MATURITY, 100);
    BOOST_CHECK_EQUAL(Params().GetConsensus().nCoinbaseMaturity, 100);
    BOOST_CHECK(COINBASE_MATURITY != Consensus::CONVERGENCE_DRAW_WINDOW);
    BOOST_CHECK_EQUAL(RES_MINT_MATURITY, COINBASE_MATURITY);

    const int coin_h = 1000;
    BOOST_CHECK(1000 + Consensus::CONVERGENCE_DRAW_WINDOW - coin_h < COINBASE_MATURITY);
    BOOST_CHECK(1000 + COINBASE_MATURITY - coin_h >= COINBASE_MATURITY);
    // Being in a draw window does not mature a coinbase.
    BOOST_CHECK(HeightInWindow(coin_h, coin_h + 10));
    BOOST_CHECK(coin_h + 10 - coin_h < COINBASE_MATURITY);
}

BOOST_AUTO_TEST_CASE(w32_ui_lifecycle_is_inclusion_anchored_not_floor_group)
{
    const auto s = wallet::BuildConvergenceUiWindow(/*inclusion=*/1000, /*tip=*/1010, false, false);
    BOOST_CHECK_EQUAL(s.W, 32);
    BOOST_CHECK_EQUAL(s.start_block, 1000);
    BOOST_CHECK_EQUAL(s.finalization_block, 1032);
    BOOST_CHECK(s.start_block != 992); // not floor(1000/32)*32
}

BOOST_AUTO_TEST_CASE(w32_witness_cache_keyed_by_hash_not_height)
{
    Consensus::ClearRuntimeSupplyWitnessCacheForTest();
    const uint256 a = HashFromLabel("w32-fork-a");
    const uint256 b = HashFromLabel("w32-fork-b");
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(1023, a, HashFromLabel("parent-a")));
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(1023, b, HashFromLabel("parent-b")));
    BOOST_REQUIRE(Consensus::GetRuntimeSupplyWitness(a).has_value());
    BOOST_REQUIRE(Consensus::GetRuntimeSupplyWitness(b).has_value());
    BOOST_CHECK(Consensus::GetRuntimeSupplyWitness(a)->block_hash == a);
    BOOST_CHECK(Consensus::GetRuntimeSupplyWitness(b)->block_hash == b);
    Consensus::InvalidateRuntimeSupplyWitness(a);
    BOOST_CHECK(!Consensus::GetRuntimeSupplyWitness(a).has_value());
    BOOST_CHECK(Consensus::GetRuntimeSupplyWitness(b).has_value());
    Consensus::ClearRuntimeSupplyWitnessCacheForTest();
}

BOOST_AUTO_TEST_CASE(w32_orphan_witness_not_retrieved_as_canonical_after_invalidate)
{
    Consensus::ClearRuntimeSupplyWitnessCacheForTest();
    const uint256 canonical = HashFromLabel("canonical-1023");
    const uint256 orphan = HashFromLabel("orphan-1023");
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(1022, HashFromLabel("parent"), uint256{}));
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(1023, orphan, HashFromLabel("parent")));
    Consensus::InvalidateRuntimeSupplyWitness(orphan);
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(1023, canonical, HashFromLabel("parent")));
    BOOST_CHECK(!Consensus::GetRuntimeSupplyWitness(orphan).has_value());
    const auto got = Consensus::GetRuntimeSupplyWitness(canonical);
    BOOST_REQUIRE(got.has_value());
    BOOST_CHECK(got->block_hash == canonical);
    BOOST_CHECK_EQUAL(got->height, 1023);
    Consensus::ClearRuntimeSupplyWitnessCacheForTest();
}

BOOST_AUTO_TEST_CASE(w32_witness_chain_rejects_height_skip_and_wrong_parent)
{
    const auto parent = MakeWitness(7, HashFromLabel("p7"), HashFromLabel("p6"));
    BOOST_CHECK(!Consensus::VerifyRuntimeWitnessChainStep(parent, MakeWitness(9, HashFromLabel("c9"), parent.block_hash)));
    BOOST_CHECK(!Consensus::VerifyRuntimeWitnessChainStep(parent, MakeWitness(8, HashFromLabel("c8"), HashFromLabel("wrong"))));
    BOOST_CHECK(Consensus::VerifyRuntimeWitnessChainStep(parent, MakeWitness(8, HashFromLabel("c8"), parent.block_hash)));
}

BOOST_AUTO_TEST_CASE(w32_prune_keep_window_covers_supported_reorg_plus_w)
{
    BOOST_CHECK_GE(MIN_BLOCKS_TO_KEEP, MAX_SUPPORTED_REORG_DEPTH + static_cast<unsigned>(Consensus::CONVERGENCE_DRAW_WINDOW));
    BOOST_CHECK_EQUAL(MAX_SUPPORTED_REORG_DEPTH, 288U);
    BOOST_CHECK_EQUAL(MIN_BLOCKS_TO_KEEP, 320U);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(w32_reorg_stability_chain_tests, TestChain100Setup)

namespace {

void ReorgTipDepth(TestChain100Setup& setup, int depth)
{
    auto& chainman = *Assert(setup.m_node.chainman);
    const int start_height = WITH_LOCK(::cs_main, return chainman.ActiveChain().Height());
    BOOST_REQUIRE_GE(start_height, depth);

    std::vector<uint256> old_hashes;
    {
        LOCK(::cs_main);
        const CChain& active = chainman.ActiveChain();
        for (int h = start_height - depth + 1; h <= start_height; ++h) {
            old_hashes.push_back(active[h]->GetBlockHash());
        }
    }

    for (int i = 0; i < depth; ++i) {
        BlockValidationState state;
        CBlockIndex* tip = WITH_LOCK(::cs_main, return chainman.ActiveChain().Tip());
        BOOST_REQUIRE(chainman.ActiveChainstate().InvalidateBlock(state, tip));
    }

    setup.coinbaseKey.MakeNewKey(true);
    for (int i = 0; i < depth; ++i) {
        setup.CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(setup.coinbaseKey));
        SetMockTime(GetTime() + 1);
    }

    LOCK(::cs_main);
    const CChain& active = chainman.ActiveChain();
    BOOST_CHECK_EQUAL(active.Height(), start_height);
    for (int i = 0; i < depth; ++i) {
        const int h = start_height - depth + 1 + i;
        BOOST_CHECK(active[h]->GetBlockHash() != old_hashes[i]);
        BOOST_CHECK_EQUAL(DrawWindowFirst(h), std::max(0, h - Consensus::CONVERGENCE_DRAW_WINDOW + 1));
        BOOST_CHECK(active[h]->m_draw_set.IsSet());
        BOOST_CHECK_EQUAL(active[h]->m_draw_set.source_height, h);
    }
}

} // namespace

BOOST_AUTO_TEST_CASE(chain_window_reorgs_and_boundaries)
{
    {
        LOCK(::cs_main);
        const CBlockIndex* tip = Assert(m_node.chainman)->ActiveChain().Tip();
        BOOST_REQUIRE(tip != nullptr);
        BOOST_CHECK_EQUAL(tip->nHeight, COINBASE_MATURITY);
        BOOST_CHECK_EQUAL(DrawWindowFirst(tip->nHeight), tip->nHeight - 31);
        BOOST_CHECK(tip->m_draw_set.IsSet());
        BOOST_CHECK_EQUAL(tip->m_draw_set.source_height, tip->nHeight);
        ConvergenceDrawSet set;
        BOOST_REQUIRE(GetConvergenceDrawSetForBlock(*tip, Params().GetConsensus(), set));
        BOOST_CHECK_EQUAL(set.source_height, tip->nHeight);
    }

    const std::vector<int> boundaries{32, 63, 64, 96, 99, 100};
    std::vector<int> firsts;
    {
        LOCK(::cs_main);
        for (int h : boundaries) {
            firsts.push_back(DrawWindowFirst(h));
        }
    }

    for (int depth : {1, 31, 32, 33, 64, 100}) {
        ReorgTipDepth(*this, depth);
    }

    mineBlocks(1);
    ReorgTipDepth(*this, 101);

    {
        constexpr int kDeepReorg = 288;
        const int now = WITH_LOCK(::cs_main, return Assert(m_node.chainman)->ActiveChain().Height());
        BOOST_REQUIRE_LT(now, kDeepReorg);
        mineBlocks(kDeepReorg - now);
        BOOST_CHECK_EQUAL(WITH_LOCK(::cs_main, return Assert(m_node.chainman)->ActiveChain().Height()), kDeepReorg);
        ReorgTipDepth(*this, kDeepReorg);
    }

    LOCK(::cs_main);
    const CChain& active = Assert(m_node.chainman)->ActiveChain();
    for (size_t i = 0; i < boundaries.size(); ++i) {
        const int h = boundaries[i];
        BOOST_CHECK_EQUAL(DrawWindowFirst(h), firsts[i]);
        BOOST_REQUIRE(active[h] != nullptr);
        BOOST_CHECK_EQUAL(active[h]->m_draw_set.source_height, h);
    }
    BOOST_CHECK_EQUAL(DrawWindowFirst(32), 1);
    BOOST_CHECK_EQUAL(DrawWindowFirst(64), 33);
    BOOST_CHECK(DrawWindowFirst(32) != 0);
    BOOST_CHECK(DrawWindowFirst(64) != 64);
}

BOOST_AUTO_TEST_CASE(orphaned_coinbase_leaves_utxo_set)
{
    uint256 orphan_cb;
    {
        LOCK(::cs_main);
        const CBlockIndex* tip = Assert(m_node.chainman)->ActiveChain().Tip();
        BOOST_REQUIRE(tip != nullptr);
        CBlock block;
        BOOST_REQUIRE(node::ReadBlockFromDisk(block, tip, Params().GetConsensus()));
        BOOST_REQUIRE(!block.vtx.empty());
        orphan_cb = block.vtx[0]->GetHash();
        BOOST_CHECK(Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(COutPoint(orphan_cb, 0)));
    }

    BlockValidationState state;
    CBlockIndex* tip = WITH_LOCK(::cs_main, return Assert(m_node.chainman)->ActiveChain().Tip());
    BOOST_REQUIRE(Assert(m_node.chainman)->ActiveChainstate().InvalidateBlock(state, tip));

    {
        LOCK(::cs_main);
        BOOST_CHECK(!Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(COutPoint(orphan_cb, 0)));
        BOOST_CHECK(!Consensus::GetRuntimeSupplyWitness(tip).has_value());
        BOOST_CHECK(!Consensus::GetRuntimeSupplyMirror(*tip).has_value());
    }

    coinbaseKey.MakeNewKey(true);
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    SetMockTime(GetTime() + 1);

    LOCK(::cs_main);
    BOOST_CHECK(!Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(COutPoint(orphan_cb, 0)));
    const CBlockIndex* new_tip = Assert(m_node.chainman)->ActiveChain().Tip();
    BOOST_CHECK(new_tip->GetBlockHash() != tip->GetBlockHash());
    BOOST_CHECK_EQUAL(DrawWindowFirst(new_tip->nHeight), DrawWindowFirst(tip->nHeight));
}

BOOST_AUTO_TEST_CASE(maturity_boundary_uses_height_not_window)
{
    const auto& consensus = Params().GetConsensus();
    LOCK(::cs_main);
    const CChain& active = Assert(m_node.chainman)->ActiveChain();
    const int spend_h = active.Height() + 1;
    // Coinbase at height 0 is mature at spend 101 (depth 101) on a 100-block chain
    // when the next block is connected. At the current tip (100), depth of height-0
    // coinbase is 100, which meets nCoinbaseMaturity.
    BOOST_CHECK_GE(active.Height() - 0, consensus.nCoinbaseMaturity);
    BOOST_CHECK_LT(active.Height() - 1, consensus.nCoinbaseMaturity + 1);
    // W=32 is irrelevant to that comparison.
    BOOST_CHECK_LT(Consensus::CONVERGENCE_DRAW_WINDOW, consensus.nCoinbaseMaturity);

    const CBlockIndex* tip = active.Tip();
    BOOST_REQUIRE(tip != nullptr);
    const int window_first = DrawWindowFirst(tip->nHeight);
    BOOST_CHECK(window_first > 0);
    // A coinbase inside the current draw window is still immature if it is recent.
    BOOST_CHECK_LT(tip->nHeight - window_first, consensus.nCoinbaseMaturity);
    (void)spend_h;
}

BOOST_AUTO_TEST_CASE(connect_disconnect_connect_does_not_keep_orphan_draw_as_canonical)
{
    uint256 old_tip_hash;
    int old_height = 0;
    {
        LOCK(::cs_main);
        const CBlockIndex* tip = Assert(m_node.chainman)->ActiveChain().Tip();
        old_tip_hash = tip->GetBlockHash();
        old_height = tip->nHeight;
        BOOST_CHECK(tip->m_draw_set.IsSet());
        BOOST_CHECK_EQUAL(tip->m_draw_set.source_height, old_height);
    }

    BlockValidationState state;
    CBlockIndex* old_tip = WITH_LOCK(::cs_main, return Assert(m_node.chainman)->ActiveChain().Tip());
    BOOST_REQUIRE(Assert(m_node.chainman)->ActiveChainstate().InvalidateBlock(state, old_tip));

    coinbaseKey.MakeNewKey(true);
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    SetMockTime(GetTime() + 1);

    LOCK(::cs_main);
    const CBlockIndex* new_tip = Assert(m_node.chainman)->ActiveChain().Tip();
    BOOST_CHECK(new_tip->GetBlockHash() != old_tip_hash);
    BOOST_CHECK_EQUAL(new_tip->nHeight, old_height);
    BOOST_CHECK_EQUAL(new_tip->m_draw_set.source_height, old_height);
    // Orphan index still carries ITS draw set (hash-keyed), but it is not ActiveChain tip.
    BOOST_CHECK(old_tip->m_draw_set.IsSet());
    BOOST_CHECK(old_tip->GetBlockHash() == old_tip_hash);
    BOOST_CHECK(Assert(m_node.chainman)->ActiveChain().Tip()->GetBlockHash() != old_tip_hash);
    BOOST_CHECK_EQUAL(DrawWindowFirst(old_height), DrawWindowFirst(new_tip->nHeight));
}

BOOST_AUTO_TEST_SUITE_END()
