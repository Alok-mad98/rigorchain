// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <consensus/validation.h>
#include <span.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <validation.h>
#include <version.h>

#include <boost/test/unit_test.hpp>

#include <stdexcept>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(paid_winner_index_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(disk_block_index_roundtrips_paid_winner_txid)
{
    uint256 paid;
    paid.SetHex("0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");

    CDiskBlockIndex disk;
    disk.nHeight = 7;
    disk.nStatus = 0;
    disk.nTx = 1;
    disk.nVersion = 4;
    disk.nTime = 1;
    disk.nBits = 0x1d00ffff;
    disk.nNonce = 1;
    disk.m_paid_winner_txid = paid;
    disk.m_block_res_mint_sum = 1000;
    disk.m_block_converge_sum = 0;

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    {
        LOCK(cs_main);
        ss << disk;
    }

    CDiskBlockIndex loaded;
    {
        LOCK(cs_main);
        ss >> loaded;
    }
    BOOST_CHECK(loaded.m_paid_winner_txid == paid);
    BOOST_CHECK_EQUAL(loaded.nHeight, 7);
    BOOST_CHECK_EQUAL(loaded.m_block_res_mint_sum, 1000);
    BOOST_CHECK(!loaded.m_draw_set.IsSet());
}

BOOST_AUTO_TEST_CASE(disk_block_index_roundtrips_draw_set)
{
    ConvergenceDrawCandidate candidate;
    candidate.txid.SetHex("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    candidate.weight = 1024;
    candidate.res_payout_hash160.SetHex("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    candidate.source_height = 12;

    CDiskBlockIndex disk;
    disk.nHeight = 12;
    disk.nStatus = 0;
    disk.nTx = 1;
    disk.nVersion = 4;
    disk.nTime = 1;
    disk.nBits = 0x1d00ffff;
    disk.nNonce = 1;
    disk.m_draw_set.source_block_hash.SetHex("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    disk.m_draw_set.source_height = 12;
    disk.m_draw_set.source_nbits = 0x1d00ffff;
    disk.m_draw_set.convergence_root.SetHex("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    disk.m_draw_set.candidates.push_back(candidate);

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    {
        LOCK(cs_main);
        ss << disk;
    }

    CDiskBlockIndex loaded;
    {
        LOCK(cs_main);
        ss >> loaded;
    }
    BOOST_REQUIRE(loaded.m_draw_set.IsSet());
    BOOST_CHECK(loaded.m_draw_set.source_block_hash == disk.m_draw_set.source_block_hash);
    BOOST_CHECK_EQUAL(loaded.m_draw_set.source_height, 12);
    BOOST_CHECK_EQUAL(loaded.m_draw_set.source_nbits, 0x1d00ffff);
    BOOST_CHECK(loaded.m_draw_set.convergence_root == disk.m_draw_set.convergence_root);
    BOOST_REQUIRE_EQUAL(loaded.m_draw_set.candidates.size(), 1U);
    BOOST_CHECK(loaded.m_draw_set.candidates[0].txid == candidate.txid);
    BOOST_CHECK_EQUAL(loaded.m_draw_set.candidates[0].weight, 1024);
    BOOST_CHECK(loaded.m_draw_set.candidates[0].res_payout_hash160 == candidate.res_payout_hash160);
    BOOST_CHECK_EQUAL(loaded.m_draw_set.candidates[0].source_height, 12);
}

BOOST_AUTO_TEST_CASE(truncated_index_without_paid_winner_fails_closed)
{
    CDiskBlockIndex disk;
    disk.nHeight = 3;
    disk.nStatus = 0;
    disk.nTx = 1;
    disk.nVersion = 4;
    disk.nTime = 1;
    disk.nBits = 0x1d00ffff;
    disk.nNonce = 1;

    CDataStream ss(SER_DISK, CLIENT_VERSION);
    {
        LOCK(cs_main);
        ss << disk;
    }
    BOOST_REQUIRE(ss.size() > 32);
    std::vector<std::byte> truncated(ss.begin(), ss.end() - 32);

    CDataStream short_ss{Span<const std::byte>{truncated}, SER_DISK, CLIENT_VERSION};
    CDiskBlockIndex loaded;
    bool threw = false;
    try {
        LOCK(cs_main);
        short_ss >> loaded;
    } catch (const std::ios_base::failure&) {
        threw = true;
    }
    BOOST_CHECK(threw);
}

BOOST_AUTO_TEST_CASE(connected_empty_blocks_record_null_paid_winner)
{
    LOCK(cs_main);
    const CBlockIndex* tip = Assert(m_node.chainman)->ActiveChain().Tip();
    BOOST_REQUIRE(tip != nullptr);
    // TestChain100Setup mines coinbase-only blocks: no RIGOR_MARKER_RES_MINT, so the record
    // must stay null. A non-null record here would exclude a phantom ticket from F.
    for (const CBlockIndex* p = tip; p != nullptr; p = p->pprev) {
        BOOST_CHECK_MESSAGE(p->m_paid_winner_txid.IsNull(),
            strprintf("height %d recorded a paid winner on a coinbase-only chain", p->nHeight));
        BOOST_CHECK_EQUAL(p->has_res, 0);
        BOOST_CHECK_MESSAGE(p->m_draw_set.IsSet(),
            strprintf("height %d has no persisted draw set", p->nHeight));
        BOOST_CHECK_EQUAL(p->m_draw_set.source_height, p->nHeight);
        BOOST_CHECK(p->m_draw_set.candidates.empty());
    }
}

BOOST_AUTO_TEST_CASE(draw_set_read_on_tip_is_bounded_and_complete)
{
    LOCK(cs_main);
    const CBlockIndex* tip = Assert(m_node.chainman)->ActiveChain().Tip();
    BOOST_REQUIRE(tip != nullptr);
    ConvergenceDrawSet set;
    BOOST_REQUIRE(GetConvergenceDrawSetForBlock(*tip, Params().GetConsensus(), set));
    BOOST_CHECK(set.IsSet());
    BOOST_CHECK_EQUAL(set.source_height, tip->nHeight);
    BOOST_CHECK(set.candidates.empty());
    BOOST_CHECK(set.convergence_root.IsNull());
}

BOOST_AUTO_TEST_CASE(persisted_draw_set_readable_without_block_data)
{
    LOCK(cs_main);
    CBlockIndex* tip = Assert(m_node.chainman)->ActiveChain().Tip();
    BOOST_REQUIRE(tip != nullptr);
    BOOST_REQUIRE(tip->m_draw_set.IsSet());
    const uint32_t saved_status = tip->nStatus;
    tip->nStatus &= ~(BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO);
    ConvergenceDrawSet set;
    BOOST_REQUIRE(GetConvergenceDrawSetForBlock(*tip, Params().GetConsensus(), set));
    BOOST_CHECK_EQUAL(set.source_height, tip->nHeight);
    BOOST_CHECK(set.candidates.empty());
    tip->nStatus = saved_status;
}

BOOST_AUTO_TEST_SUITE_END()
