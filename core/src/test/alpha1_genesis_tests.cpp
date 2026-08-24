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

// RFC-016 §2.2 — Alpha1 mainnet genesis identity (v0.1.1-alpha).
const uint256 ALPHA1_GENESIS_HASH =
    uint256S("0x1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758");
// v0.1.0-alpha / 055b866 historical genesis — foreign to Alpha1.
const uint256 LEGACY_V010_GENESIS_HASH =
    uint256S("0x5dd90c30e88b90a1a6ce0b371d4abf8d7283d8a1275f70f9d51a8821048221de");
// Coinbase-determined merkle root; identical to v0.1.0-alpha (pszTimestamp, subsidy, scriptPubKey unchanged).
const uint256 ALPHA1_GENESIS_MERKLE =
    uint256S("0xccdba6059e3d93d9f9eddc83e17cf358d8181155d39ed4b0526d8cd4439528a8");

constexpr uint32_t ALPHA1_NTIME = 1786125600; // 2026-08-07 18:00:00 UTC
constexpr uint32_t ALPHA1_NBITS = 0x1e010000;
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
    // Header-sync bootstrap floor: 2^26. Four genesis-difficulty (0x1e010000 ≈ 2^24)
    // blocks pass the gate; a 2000-header powLimit flood does not. RFC-016 §2.4
    // originally pinned 0x100000 (below one honest block); that floor is too low
    // against powLimit headers and was raised in CMainParams.
    BOOST_CHECK_EQUAL(
        consensus.nMinimumChainWork,
        uint256S("0x0000000000000000000000000000000000000000000000000000000004000000"));

    const auto& checkpoints = chainParams->Checkpoints().mapCheckpoints;
    BOOST_REQUIRE_EQUAL(checkpoints.size(), 1U);
    BOOST_CHECK_EQUAL(checkpoints.at(0), consensus.hashGenesisBlock);

    BOOST_REQUIRE_EQUAL(genesis.vtx.size(), 1U);
    const CTransaction& coinbase = *genesis.vtx[0];
    BOOST_REQUIRE_EQUAL(coinbase.vin.size(), 1U);
    BOOST_REQUIRE_EQUAL(coinbase.vout.size(), 1U);
    BOOST_CHECK_EQUAL(coinbase.vout[0].nValue, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(coinbase.vout[0].nValue, 1000 * COIN);
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
    // RFC-016 §8: testnet / signet / regtest / devtest / slowtest hashes stay as frozen
    // in genesis-lockdown.md. Alpha1 only replaces mainnet identity.
    const auto testnet = CreateChainParams(*m_node.args, CBaseChainParams::TESTNET);
    BOOST_CHECK_EQUAL(
        testnet->GetConsensus().hashGenesisBlock,
        uint256S("0xdc14491196188457f5e2d9f5951e38b788a1c714dd606225b7490dafa9ef8bca"));

    const auto signet = CreateChainParams(*m_node.args, CBaseChainParams::SIGNET);
    BOOST_CHECK_EQUAL(
        signet->GetConsensus().hashGenesisBlock,
        uint256S("0x85b91b276d162f9fe04fec1f24ecf84c46bd027d75e4be1943412d221ff8f9a7"));

    const auto regtest = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    BOOST_CHECK_EQUAL(
        regtest->GetConsensus().hashGenesisBlock,
        uint256S("0xcc31d596fd01d27134f1ad83e50dc93699c984683acc420477ad986ffa5f348e"));

    const auto devtest = CreateChainParams(*m_node.args, CBaseChainParams::DEVTEST);
    BOOST_CHECK_EQUAL(
        devtest->GetConsensus().hashGenesisBlock,
        uint256S("0x407381b8179c92027826909806920e3633d5487cac816e8fff419223d30641c7"));

    const auto slowtest = CreateChainParams(*m_node.args, CBaseChainParams::SLOWTEST);
    BOOST_CHECK_EQUAL(slowtest->GenesisBlock().nTime, 1777177700U);
    BOOST_CHECK_EQUAL(slowtest->GenesisBlock().nBits, 0x1e0fffffU);
    BOOST_CHECK_EQUAL(slowtest->GenesisBlock().nNonce, 0U);
}

BOOST_AUTO_TEST_SUITE_END()
