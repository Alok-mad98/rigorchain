// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/issuance_height_boundary_p0_040.json.h>

#include <algorithm>
#include <univalue.h>

#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergenceres.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <node/blockstorage.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <sync.h>
#include <node/supply_monitor/runtime_supply_state.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

namespace {

const uint256 MAINNET_GENESIS_HASH =
    uint256S("0xf8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717");
const uint256 MAINNET_GENESIS_MERKLE =
    uint256S("0x9d3fbd207af927fc8e2c57e975d8d5845913bf059d956592e8a0bcf38b3515dd");
const uint256 PRE_P0038_MAINNET_GENESIS_HASH =
    uint256S("0x1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758");

CBlock GenesisWithReward(const CBlock& src, CAmount reward)
{
    CBlock out = src;
    BOOST_REQUIRE(!out.vtx.empty());
    CMutableTransaction tx(*out.vtx[0]);
    BOOST_REQUIRE_EQUAL(tx.vout.size(), 1U);
    tx.vout[0].nValue = reward;
    out.vtx[0] = MakeTransactionRef(std::move(tx));
    out.hashMerkleRoot = BlockMerkleRoot(out);
    return out;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(genesis_zero_issuance_p0_037_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(frozen_p0_parameters_unchanged)
{
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW, 32);
    BOOST_CHECK_EQUAL(COINBASE_MATURITY, 100);
    BOOST_CHECK_EQUAL(RES_MINT_MATURITY, 100);
    BOOST_CHECK_EQUAL(Consensus::ORU_INITIAL_BLOCK_SUBSIDY, 1000 * COIN);
    BOOST_CHECK_EQUAL(Consensus::ORU_GENESIS_BLOCK_SUBSIDY, 0);
    BOOST_CHECK_EQUAL(Consensus::ORU_MIN_BLOCK_SUBSIDY, 125 * COIN);
    BOOST_CHECK_EQUAL(Consensus::RES_REWARD_GENESIS, 40 * Consensus::RES_COIN);
    BOOST_CHECK_EQUAL(Consensus::GetResMintReward(0), Consensus::RES_REWARD_GENESIS);
    BOOST_CHECK_EQUAL(Consensus::GetResMintReward(1), Consensus::RES_REWARD_GENESIS);
    BOOST_CHECK(Consensus::RES_PROBABILITY_TARGET_CONST ==
                uint256S("000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479"));
}

BOOST_AUTO_TEST_CASE(genesis_reward_zero)
{
    const auto& consensus = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, consensus), 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, consensus), Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(height_1_reward_unchanged)
{
    const auto& consensus = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, consensus), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    // After genesis=0, GetBlockSubsidy(0)+1000 equals the legitimate height-1
    // reward. Compensation would instead double the era-0 rate to 2000.
    BOOST_CHECK_NE(GetBlockSubsidy(1, consensus), 2 * Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(height_2_reward_unchanged)
{
    const auto& consensus = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2, consensus), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK_NE(GetBlockSubsidy(2, consensus), 2000 * COIN);
}

BOOST_AUTO_TEST_CASE(genesis_hash_matches_frozen_vector)
{
    const CBlock& genesis = Params().GenesisBlock();
    BOOST_CHECK_EQUAL(genesis.GetHash(), MAINNET_GENESIS_HASH);
    BOOST_CHECK_EQUAL(Params().GetConsensus().hashGenesisBlock, MAINNET_GENESIS_HASH);
    BOOST_CHECK(genesis.GetHash() != PRE_P0038_MAINNET_GENESIS_HASH);
}

BOOST_AUTO_TEST_CASE(genesis_merkle_matches_frozen_vector)
{
    const CBlock& genesis = Params().GenesisBlock();
    BOOST_CHECK_EQUAL(genesis.hashMerkleRoot, MAINNET_GENESIS_MERKLE);
    BOOST_CHECK_EQUAL(BlockMerkleRoot(genesis), MAINNET_GENESIS_MERKLE);
}

BOOST_AUTO_TEST_CASE(genesis_coinbase_value_zero)
{
    const CBlock& genesis = Params().GenesisBlock();
    BOOST_REQUIRE_EQUAL(genesis.vtx.size(), 1U);
    const CTransaction& coinbase = *genesis.vtx[0];
    BOOST_REQUIRE_EQUAL(coinbase.vout.size(), 1U);
    BOOST_CHECK_EQUAL(coinbase.vout[0].nValue, 0);
    BOOST_CHECK_EQUAL(Consensus::GetOutputValueByAsset(coinbase, ASSET_ORU), 0);
    BOOST_CHECK_EQUAL(Consensus::GetOutputValueByAsset(coinbase, ASSET_RES), 0);
}

BOOST_AUTO_TEST_CASE(genesis_no_hidden_allocation)
{
    const CBlock& genesis = Params().GenesisBlock();
    BOOST_CHECK_EQUAL(genesis.vtx.size(), 1U);
    BOOST_CHECK_EQUAL(genesis.vtx[0]->vin.size(), 1U);
    BOOST_CHECK_EQUAL(genesis.vtx[0]->vout.size(), 1U);
    BOOST_CHECK_EQUAL(HexStr(genesis.vtx[0]->vout[0].scriptPubKey),
                      "00141111111111111111111111111111111111111111");
}

BOOST_AUTO_TEST_CASE(genesis_no_res_mint)
{
    const CBlock& genesis = Params().GenesisBlock();
    BOOST_CHECK(!CoinbaseHasResMint(*genesis.vtx[0]));
}

BOOST_AUTO_TEST_CASE(genesis_no_oru_utxo)
{
    const CBlock& genesis = Params().GenesisBlock();
    const COutPoint genesis_out(genesis.vtx[0]->GetHash(), 0);
    LOCK(::cs_main);
    const auto& coins = Assert(m_node.chainman)->ActiveChainstate().CoinsTip();
    BOOST_CHECK(!coins.HaveCoin(genesis_out));
    BOOST_CHECK_EQUAL(Assert(m_node.chainman)->ActiveChain().Height(), 0);
}

BOOST_AUTO_TEST_CASE(genesis_reward_nonzero_is_not_frozen)
{
    const CBlock& genesis = Params().GenesisBlock();
    const CBlock nonzero = GenesisWithReward(genesis, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK(nonzero.GetHash() != MAINNET_GENESIS_HASH);
    BOOST_CHECK(nonzero.hashMerkleRoot != MAINNET_GENESIS_MERKLE);
    BOOST_CHECK_EQUAL(genesis.vtx[0]->vout[0].nValue, 0);
}

BOOST_AUTO_TEST_CASE(genesis_hidden_allocation_rejected_by_construction)
{
    const CBlock& genesis = Params().GenesisBlock();
    CBlock extra = genesis;
    CMutableTransaction tx(*extra.vtx[0]);
    tx.vout.push_back(CTxOut(1000 * COIN, tx.vout[0].scriptPubKey));
    extra.vtx[0] = MakeTransactionRef(std::move(tx));
    extra.hashMerkleRoot = BlockMerkleRoot(extra);
    BOOST_CHECK_NE(extra.vtx[0]->vout.size(), 1U);
    BOOST_CHECK(extra.GetHash() != Params().GetConsensus().hashGenesisBlock);
    BOOST_CHECK(extra.hashMerkleRoot != MAINNET_GENESIS_MERKLE);
}

BOOST_AUTO_TEST_CASE(genesis_mint_nonzero_is_not_the_frozen_genesis)
{
    const CBlock& genesis = Params().GenesisBlock();
    BOOST_CHECK(!CoinbaseHasResMint(*genesis.vtx[0]));
    BOOST_CHECK_EQUAL(Consensus::GetOutputValueByAsset(*genesis.vtx[0], ASSET_RES), 0);
}

BOOST_AUTO_TEST_CASE(genesis_supply_ledger_matches_utxo)
{
    const auto& consensus = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, consensus), 0);
    const COutPoint genesis_out(Params().GenesisBlock().vtx[0]->GetHash(), 0);
    LOCK(::cs_main);
    BOOST_CHECK(!Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(genesis_out));
}

BOOST_AUTO_TEST_CASE(all_shipped_networks_genesis_reward_zero)
{
    const std::vector<std::string> nets = {
        CBaseChainParams::MAIN,
        CBaseChainParams::TESTNET,
        CBaseChainParams::REGTEST,
        CBaseChainParams::DEVTEST,
        CBaseChainParams::SLOWTEST,
    };
    for (const auto& net : nets) {
        const auto params = CreateChainParams(*m_node.args, net);
        BOOST_CHECK_EQUAL(params->GenesisBlock().vtx[0]->vout[0].nValue, 0);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(0, params->GetConsensus()), 0);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(1, params->GetConsensus()),
                          params->GetConsensus().nBlockSubsidyBase);
    }
}

BOOST_AUTO_TEST_CASE(issuance_epoch_is_height_div_interval)
{
    const auto& c = Params().GetConsensus();
    BOOST_REQUIRE_EQUAL(c.nSubsidyHalvingInterval, Consensus::ORU_SUBSIDY_HALVING_INTERVAL);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(209999, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(210000, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 1);
    BOOST_CHECK_NE(GetBlockSubsidy(210000, c), GetBlockSubsidy(209999, c));
    BOOST_CHECK_EQUAL(GetBlockSubsidy(210001, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 1);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(419999, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 1);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420000, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420001, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(629999, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 2);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(630000, c), Consensus::ORU_MIN_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(oru_res_halving_boundary_matches)
{
    const int heights[] = {0, 1, 2, 209998, 209999, 210000, 210001, 210002,
                           419999, 420000, 420001, 629999, 630000, 630001};
    const auto& c = Params().GetConsensus();
    BOOST_REQUIRE_EQUAL(c.nSubsidyHalvingInterval, Consensus::ORU_SUBSIDY_HALVING_INTERVAL);
    for (int h : heights) {
        const int epoch = h / Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
        BOOST_CHECK_EQUAL(epoch, h / Consensus::RES_REWARD_EPOCH);
        BOOST_CHECK_EQUAL(Consensus::GetResMintReward(h), Consensus::ResRewardForEpoch(epoch));
        if (h == 0) {
            BOOST_CHECK_EQUAL(GetBlockSubsidy(h, c), 0);
        } else {
            const int shifts = std::min(epoch, Consensus::ORU_SUBSIDY_MAX_SHIFTS);
            BOOST_CHECK_EQUAL(GetBlockSubsidy(h, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> shifts);
        }
    }
}

BOOST_AUTO_TEST_CASE(issuance_boundary_golden_vectors)
{
    UniValue root;
    const std::string raw(
        json_tests::issuance_height_boundary_p0_040,
        json_tests::issuance_height_boundary_p0_040 + sizeof(json_tests::issuance_height_boundary_p0_040));
    BOOST_REQUIRE(root.read(raw));
    const auto& c = Params().GetConsensus();
    BOOST_REQUIRE_EQUAL(c.nSubsidyHalvingInterval, Consensus::ORU_SUBSIDY_HALVING_INTERVAL);
    const UniValue& vectors = root["vectors"];
    for (size_t i = 0; i < vectors.size(); ++i) {
        const UniValue& v = vectors[i];
        const int h = v["height"].getInt<int>();
        BOOST_CHECK_EQUAL(GetBlockSubsidy(h, c), v["oru_reward"].getInt<int64_t>());
        BOOST_CHECK_EQUAL(Consensus::GetResMintReward(h), v["res_reward"].getInt<int64_t>());
        BOOST_CHECK_EQUAL(h / Consensus::ORU_SUBSIDY_HALVING_INTERVAL, v["epoch"].getInt<int>());
        if (v.exists("res_epoch")) {
            BOOST_CHECK_EQUAL(h / Consensus::RES_REWARD_EPOCH, v["res_epoch"].getInt<int>());
        }
        if (v.exists("cumulative_oru")) {
            BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(h, c),
                              v["cumulative_oru"].getInt<int64_t>());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(genesis_zero_issuance_p0_037_mined_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(height_1_mined_reward_unchanged)
{
    BOOST_REQUIRE_GE(m_coinbase_txns.size(), 2U);
    BOOST_CHECK_EQUAL(m_coinbase_txns[0]->vout[0].nValue, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK_NE(m_coinbase_txns[0]->vout[0].nValue, 2000 * COIN);
}

BOOST_AUTO_TEST_CASE(height_2_mined_reward_unchanged)
{
    BOOST_REQUIRE_GE(m_coinbase_txns.size(), 2U);
    BOOST_CHECK_EQUAL(m_coinbase_txns[1]->vout[0].nValue, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK_NE(m_coinbase_txns[1]->vout[0].nValue, 2000 * COIN);
}

BOOST_AUTO_TEST_CASE(mined_genesis_still_absent_from_utxo)
{
    const CBlock& genesis = Params().GenesisBlock();
    const COutPoint genesis_out(genesis.vtx[0]->GetHash(), 0);
    LOCK(::cs_main);
    BOOST_CHECK(!Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(genesis_out));
    BOOST_CHECK(Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(
        COutPoint(m_coinbase_txns[0]->GetHash(), 0)));
}

BOOST_AUTO_TEST_SUITE_END()
