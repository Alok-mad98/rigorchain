// Copyright (c) 2026 The RIGOR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <chainparamsbase.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(alpha1_genesis_tests, BasicTestingSetup)

namespace {

// P0-038 — mainnet genesis identity (ORU reward 0). Pre-P0-038 1c2d6436…d758 is foreign.
const uint256 ALPHA1_GENESIS_HASH =
    uint256S("0xf8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717");
// v0.1.0-alpha / 055b866 historical genesis — foreign to Alpha1.
const uint256 LEGACY_V010_GENESIS_HASH =
    uint256S("0x5dd90c30e88b90a1a6ce0b371d4abf8d7283d8a1275f70f9d51a8821048221de");
const uint256 PRE_P0038_GENESIS_HASH =
    uint256S("0x1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758");
const uint256 ALPHA1_GENESIS_MERKLE =
    uint256S("0x9d3fbd207af927fc8e2c57e975d8d5845913bf059d956592e8a0bcf38b3515dd");

constexpr uint32_t ALPHA1_NTIME = 1786125600; // 2026-08-07 18:00:00 UTC
constexpr uint32_t ALPHA1_NBITS = 0x1d008000;
constexpr uint32_t ALPHA1_NNONCE = 0;
constexpr int32_t ALPHA1_NVERSION = 1;

const char* const ALPHA1_COINBASE_MSG =
    "Selection not computation. Convergence defines state. Naming is consensus.";

std::string LastScriptPush(const CScript& script)
{
    opcodetype opcode;
    std::vector<unsigned char> data;
    std::vector<unsigned char> last;
    CScript::const_iterator pc = script.begin();
    while (script.GetOp(pc, opcode, data)) {
        last = data;
    }
    return std::string(last.begin(), last.end());
}

} // namespace

BOOST_AUTO_TEST_CASE(alpha1_genesis_hash_matches_rfc016)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis = chainParams->GenesisBlock();

    BOOST_CHECK_EQUAL(genesis.GetHash(), ALPHA1_GENESIS_HASH);
    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock, ALPHA1_GENESIS_HASH);
    BOOST_CHECK(consensus.hashGenesisBlock != LEGACY_V010_GENESIS_HASH);
    BOOST_CHECK(consensus.hashGenesisBlock != PRE_P0038_GENESIS_HASH);
}

BOOST_AUTO_TEST_CASE(alpha1_genesis_timestamp)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(chainParams->GenesisBlock().nTime, ALPHA1_NTIME);
}

BOOST_AUTO_TEST_CASE(alpha1_genesis_nbits)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(chainParams->GenesisBlock().nBits, ALPHA1_NBITS);
}

BOOST_AUTO_TEST_CASE(alpha1_genesis_nonce)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(chainParams->GenesisBlock().nNonce, ALPHA1_NNONCE);
}

BOOST_AUTO_TEST_CASE(alpha1_chainparams_consistency)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis = chainParams->GenesisBlock();

    BOOST_CHECK_EQUAL(genesis.nVersion, ALPHA1_NVERSION);
    BOOST_CHECK(genesis.hashPrevBlock.IsNull());
    BOOST_CHECK_EQUAL(genesis.hashMerkleRoot, BlockMerkleRoot(genesis));
    BOOST_CHECK_EQUAL(genesis.hashMerkleRoot, ALPHA1_GENESIS_MERKLE);
    BOOST_CHECK_EQUAL(genesis.GetHash(), consensus.hashGenesisBlock);
    BOOST_CHECK_EQUAL(consensus.defaultAssumeValid, consensus.hashGenesisBlock);
    // Header-sync bootstrap floor: 2^35. Four genesis-difficulty (0x1d008000 ≈ 2^33)
    // blocks pass the gate. Header count is not a unit of this invariant (SPEC-008 §8.3).
    BOOST_CHECK_EQUAL(
        consensus.nMinimumChainWork,
        uint256S("0x0000000000000000000000000000000000000000000000000000000800000000"));

    const auto& checkpoints = chainParams->Checkpoints().mapCheckpoints;
    BOOST_REQUIRE_EQUAL(checkpoints.size(), 1U);
    BOOST_CHECK_EQUAL(checkpoints.at(0), consensus.hashGenesisBlock);

    BOOST_REQUIRE_EQUAL(genesis.vtx.size(), 1U);
    const CTransaction& coinbase = *genesis.vtx[0];
    BOOST_REQUIRE_EQUAL(coinbase.vin.size(), 1U);
    BOOST_REQUIRE_EQUAL(coinbase.vout.size(), 1U);
    BOOST_CHECK_EQUAL(coinbase.vout[0].nValue, Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(coinbase.vout[0].nValue, 0);
    BOOST_CHECK_EQUAL(LastScriptPush(coinbase.vin[0].scriptSig), ALPHA1_COINBASE_MSG);
    BOOST_CHECK_EQUAL(HexStr(coinbase.vout[0].scriptPubKey), "00141111111111111111111111111111111111111111");

    const auto& magic = chainParams->MessageStart();
    BOOST_CHECK_EQUAL(static_cast<unsigned>(magic[0]), 0xceu);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(magic[1]), 0xf1u);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(magic[2]), 0xdbu);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(magic[3]), 0xfau);
    BOOST_CHECK_EQUAL(chainParams->GetDefaultPort(), 8760);
    BOOST_CHECK_EQUAL(chainParams->Bech32HRP(), "rg");
}

BOOST_AUTO_TEST_CASE(alpha1_does_not_regenerate_other_network_genesis)
{
    // RFC-016 §8: testnet / regtest / devtest / slowtest hashes stay as frozen
    // in genesis-lockdown.md. Alpha1 only replaces mainnet identity.
    const auto testnet = CreateChainParams(*m_node.args, CBaseChainParams::TESTNET);
    BOOST_CHECK_EQUAL(
        testnet->GetConsensus().hashGenesisBlock,
        uint256S("0x0aca439758fba7fa5190b7e1cf37f9447516ef4cade07a49e7d484ea5b67b9ce"));

    const auto regtest = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    BOOST_CHECK_EQUAL(
        regtest->GetConsensus().hashGenesisBlock,
        uint256S("0x3070f47e2c1b153a3288382f3258396bb48b016eab76eae9f235d006dcc2af63"));

    const auto devtest = CreateChainParams(*m_node.args, CBaseChainParams::DEVTEST);
    BOOST_CHECK_EQUAL(
        devtest->GetConsensus().hashGenesisBlock,
        uint256S("0x9762f59ecfe1050ca45e404dc95e32ae5a93b9a046e116f2f2f562e44f21dce0"));

    const auto slowtest = CreateChainParams(*m_node.args, CBaseChainParams::SLOWTEST);
    BOOST_CHECK_EQUAL(
        slowtest->GetConsensus().hashGenesisBlock,
        uint256S("0x18ddf722cf24b35ad9e3b548ddba22a2e54535fd5932d4d699fa9602f6d3efb8"));
    BOOST_CHECK_EQUAL(slowtest->GenesisBlock().nTime, 1777177700U);
    BOOST_CHECK_EQUAL(slowtest->GenesisBlock().nBits, 0x1e0fffffU);
    BOOST_CHECK_EQUAL(slowtest->GenesisBlock().nNonce, 0U);
}

BOOST_AUTO_TEST_SUITE_END()
