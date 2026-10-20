// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chain.h>
#include <chainparams.h>
#include <node/gbt_startup_policy.h>
#include <pow.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(gbt_startup_policy_tests, BasicTestingSetup)

static node::GbtStartupFacts MainReady()
{
    node::GbtStartupFacts facts;
    facts.network_id = "main";
    const uint256 genesis = node::MainnetGenesisIdentity();
    facts.params_genesis = genesis;
    facts.genesis_block_hash = genesis;
    facts.have_active_genesis = true;
    facts.active_genesis = genesis;
    facts.handshake_peers = 1;
    facts.initial_block_download = false;
    facts.solo_mining = false;
    return facts;
}

BOOST_AUTO_TEST_CASE(minimum_chain_work_equals_genesis_proof)
{
    const auto params = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const CBlockIndex genesis_index{params->GenesisBlock()};
    const arith_uint256 proof = GetBlockProof(genesis_index);
    BOOST_CHECK(proof == arith_uint256("1ffffffff"));
    BOOST_CHECK(UintToArith256(params->GetConsensus().nMinimumChainWork) == proof);
    BOOST_CHECK_EQUAL(
        params->GetConsensus().hashGenesisBlock,
        node::MainnetGenesisIdentity());
    BOOST_CHECK(!params->IsTestChain());
    BOOST_CHECK_EQUAL(params->NetworkIDString(), "main");
}

BOOST_AUTO_TEST_CASE(no_peer_is_rejected)
{
    node::GbtStartupFacts facts = MainReady();
    facts.handshake_peers = 0;
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kNoEligiblePeer);
}

BOOST_AUTO_TEST_CASE(genesis_with_one_peer_is_not_blocked_for_low_work)
{
    // IBD is a separate fact. A genesis-only mainnet node whose tip work
    // already equals nMinimumChainWork reports initial_block_download=false.
    node::GbtStartupFacts facts = MainReady();
    facts.handshake_peers = 1;
    facts.initial_block_download = false;
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kAllow);
}

BOOST_AUTO_TEST_CASE(genesis_with_peer_still_reports_real_ibd)
{
    node::GbtStartupFacts facts = MainReady();
    facts.initial_block_download = true;
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kInitialBlockDownload);
}

BOOST_AUTO_TEST_CASE(unfinished_handshake_is_not_an_eligible_peer)
{
    // A different-magic peer never finishes verack, so the eligible count stays 0.
    node::GbtStartupFacts facts = MainReady();
    facts.handshake_peers = 0;
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kNoEligiblePeer);
}

BOOST_AUTO_TEST_CASE(genesis_hash_mismatch_is_rejected)
{
    node::GbtStartupFacts facts = MainReady();
    facts.active_genesis = uint256S("0x11");
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kGenesisMismatch);

    facts = MainReady();
    facts.params_genesis = uint256S("0x22");
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kGenesisMismatch);

    facts = MainReady();
    facts.have_active_genesis = false;
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kInitialBlockDownload);

    // Missing tip is checked before -solomining, so the flag cannot supply a template.
    facts.solo_mining = true;
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kInitialBlockDownload);
}

BOOST_AUTO_TEST_CASE(peer_disconnect_removes_eligibility)
{
    node::GbtStartupFacts facts = MainReady();
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kAllow);
    facts.handshake_peers = 0;
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kNoEligiblePeer);
}

BOOST_AUTO_TEST_CASE(solo_defaults_off_and_does_not_skip_identity)
{
    node::GbtStartupFacts facts = MainReady();
    BOOST_CHECK(!facts.solo_mining);
    facts.handshake_peers = 0;
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kNoEligiblePeer);

    facts.solo_mining = true;
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kAllow);

    facts.initial_block_download = true;
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kAllow);

    facts.active_genesis = uint256S("0x33");
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kGenesisMismatch);

    facts = MainReady();
    facts.solo_mining = true;
    facts.network_id = "test";
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kWrongNetwork);
}

BOOST_AUTO_TEST_CASE(test_chain_keeps_historical_path)
{
    node::GbtStartupFacts facts = MainReady();
    facts.is_test_chain = true;
    facts.network_id = "regtest";
    facts.handshake_peers = 0;
    facts.initial_block_download = true;
    facts.active_genesis = uint256S("0x44");
    BOOST_CHECK(node::EvaluateGbtStartup(facts) == node::GbtStartupStatus::kAllow);
}

BOOST_AUTO_TEST_SUITE_END()
