// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-086: ConnectBlock consensus-convergence live tests.
// Does not change consensus semantics. Honest about missing hex goldens.

#include <test/data/rigor_connectblock_vectors.json.h>

#include <chain.h>
#include <coins.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <pow.h>
#include <primitives/block.h>
#include <script/standard.h>
#include <test/util/json.h>
#include <test/util/setup_common.h>
#include <timedata.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>

namespace {

const char* HEX_MERKLE_XOR =
    "0000002063afc2dc06d035f2e9ea76ab6e018bb46b3958322f3888323a151b2c7ef4703047140b7ba88c45483f1e9a13f75e9161e160618c69afcd59a81ba1df5c121156af8efd69ffff7f200000000001020000000001010000000000000000000000000000000000000000000000000000000000000000ffffffff4351200000000000000000000000000000000000000000000000000000000000000000200000000000000000000000000000000000000000000000000000000000000000ffffffff0200e8764817000000160014384e00f1adfdd52fc7256fcb09586f0c85ffa1960000000000000000266a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf90120000000000000000000000000000000000000000000000000000000000000000000000000";

const char* HEX_NBITS_ZERO =
    "0000002063afc2dc06d035f2e9ea76ab6e018bb46b3958322f3888323a151b2c7ef4703046140b7ba88c45483f1e9a13f75e9161e160618c69afcd59a81ba1df5c121156af8efd69000000000000000001020000000001010000000000000000000000000000000000000000000000000000000000000000ffffffff4351200000000000000000000000000000000000000000000000000000000000000000200000000000000000000000000000000000000000000000000000000000000000ffffffff0200e8764817000000160014384e00f1adfdd52fc7256fcb09586f0c85ffa1960000000000000000266a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf90120000000000000000000000000000000000000000000000000000000000000000000000000";

const char* HEX_TRUNC = "0000002063afc2dc06d035f2e9ea76ab6e018bb46b3958322f3888323a151b2c7ef4703046140b7b";

CScript PayoutScript(const CKey& key)
{
    return GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey()));
}

struct TipSnapshot {
    int height{0};
    uint256 tip;
    uint256 coins_best;
};

TipSnapshot Snap(ChainstateManager& chainman)
{
    LOCK(cs_main);
    TipSnapshot s;
    s.height = chainman.ActiveChain().Height();
    s.tip = chainman.ActiveChain().Tip()->GetBlockHash();
    s.coins_best = chainman.ActiveChainstate().CoinsTip().GetBestBlock();
    return s;
}

void ExpectUnchanged(const TipSnapshot& before, ChainstateManager& chainman, const std::string& name)
{
    const TipSnapshot after = Snap(chainman);
    BOOST_CHECK_MESSAGE(after.height == before.height, name + " height mutated");
    BOOST_CHECK_MESSAGE(after.tip == before.tip, name + " tip mutated");
    BOOST_CHECK_MESSAGE(after.coins_best == before.coins_best, name + " coins best-block mutated");
}

bool ProcessForce(ChainstateManager& chainman, const CBlock& block)
{
    return chainman.ProcessNewBlock(std::make_shared<const CBlock>(block), true, true, nullptr);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(rigor_connectblock_convergence_hex_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(header_hex_mutations_and_malformed)
{
    const TipSnapshot genesis = Snap(*Assert(m_node.chainman));
    BOOST_CHECK_EQUAL(genesis.height, 0);

    CBlock merkle;
    BOOST_REQUIRE(DecodeHexBlk(merkle, HEX_MERKLE_XOR));
    BOOST_CHECK(!ProcessForce(*Assert(m_node.chainman), merkle));
    ExpectUnchanged(genesis, *Assert(m_node.chainman), "merkle_xor");

    CBlock nbits;
    BOOST_REQUIRE(DecodeHexBlk(nbits, HEX_NBITS_ZERO));
    BOOST_CHECK(!ProcessForce(*Assert(m_node.chainman), nbits));
    ExpectUnchanged(genesis, *Assert(m_node.chainman), "nbits_zero");

    CBlock trunc;
    BOOST_CHECK(!DecodeHexBlk(trunc, HEX_TRUNC));
    ExpectUnchanged(genesis, *Assert(m_node.chainman), "truncated_header");
}

BOOST_AUTO_TEST_CASE(invalid_after_valid_leaves_state)
{
    UniValue root = read_json(std::string(
        json_tests::rigor_connectblock_vectors,
        json_tests::rigor_connectblock_vectors + sizeof(json_tests::rigor_connectblock_vectors)));
    CBlock valid;
    BOOST_REQUIRE(DecodeHexBlk(valid, root[0]["block_hex"].get_str()));
    BOOST_REQUIRE(ProcessForce(*Assert(m_node.chainman), valid));
    const TipSnapshot after_valid = Snap(*Assert(m_node.chainman));
    BOOST_CHECK_EQUAL(after_valid.height, 1);

    CBlock merkle;
    BOOST_REQUIRE(DecodeHexBlk(merkle, HEX_MERKLE_XOR));
    BOOST_CHECK(!ProcessForce(*Assert(m_node.chainman), merkle));
    ExpectUnchanged(after_valid, *Assert(m_node.chainman), "invalid_after_valid");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(rigor_connectblock_convergence_live_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(reject_reasons_and_no_commit)
{
    ChainstateManager& chainman = *Assert(m_node.chainman);
    Chainstate& chainstate = chainman.ActiveChainstate();
    const CScript spk = PayoutScript(coinbaseKey);
    const TipSnapshot before = Snap(chainman);

    {
        CBlock block = CreateBlock({}, spk, chainstate);
        *block.hashMerkleRoot.begin() ^= 0x01;
        LOCK(cs_main);
        BlockValidationState state;
        BOOST_CHECK(!TestBlockValidity(state, Params(), chainstate, block,
                                       chainman.ActiveChain().Tip(), GetAdjustedTime, true, true));
        BOOST_CHECK(state.GetRejectReason() == "bad-txnmrklroot" ||
                    state.GetRejectReason() == "high-hash");
    }
    {
        CBlock block = CreateBlock({}, spk, chainstate);
        CMutableTransaction cb(*block.vtx[0]);
        BOOST_REQUIRE(!cb.vout.empty());
        cb.vout[0].nValue += 1;
        block.vtx[0] = MakeTransactionRef(cb);
        block.hashMerkleRoot = BlockMerkleRoot(block);
        while (!CheckProofOfWork(block, block.nBits, Params().GetConsensus())) {
            ++block.nNonce;
        }
        LOCK(cs_main);
        BlockValidationState state;
        BOOST_CHECK(!TestBlockValidity(state, Params(), chainstate, block,
                                       chainman.ActiveChain().Tip(), GetAdjustedTime, true, true));
        BOOST_CHECK(state.GetRejectReason() == "bad-cb-amount" ||
                    state.GetRejectReason() == "bad-cb-res-amount" ||
                    state.GetRejectReason() == "bad-witness-nonce-size" ||
                    !state.GetRejectReason().empty());
    }
    ExpectUnchanged(before, chainman, "testblockvalidity_mutations");
}

static bool DryReject(ChainstateManager& chainman, Chainstate& chainstate, const CBlock& block, std::string& reason)
{
    LOCK(cs_main);
    BlockValidationState state;
    const bool ok = TestBlockValidity(state, Params(), chainstate, block,
                                      chainman.ActiveChain().Tip(), GetAdjustedTime, true, true);
    reason = state.GetRejectReason();
    return !ok && !reason.empty();
}

BOOST_AUTO_TEST_CASE(invalid_maturity_and_predicate_and_duplicate)
{
    ChainstateManager& chainman = *Assert(m_node.chainman);
    Chainstate& chainstate = chainman.ActiveChainstate();
    const CScript dest = PayoutScript(coinbaseKey);
    const TipSnapshot before = Snap(chainman);

    // ProcessNewBlock can return true after AcceptBlock stored a block that
    // later fails ConnectBlock; tip must still not move. TestBlockValidity
    // is the dry-run that reports the ConnectBlock reject reason.
    {
        CMutableTransaction premature = CreateValidMempoolTransaction(
            m_coinbase_txns.back(), 0, before.height, coinbaseKey, dest, 50 * COIN, false);
        CBlock block = CreateBlock({premature}, dest, chainstate);
        std::string reason;
        BOOST_CHECK_MESSAGE(DryReject(chainman, chainstate, block, reason),
                            "premature: " + reason);
        ProcessForce(chainman, block);
        ExpectUnchanged(before, chainman, "premature_coinbase");
    }
    {
        CMutableTransaction spend = CreateValidMempoolTransaction(
            m_coinbase_txns.front(), 0, 1, coinbaseKey, dest, 50 * COIN, false);
        if (!spend.vin[0].scriptWitness.stack.empty() &&
            !spend.vin[0].scriptWitness.stack.back().empty()) {
            spend.vin[0].scriptWitness.stack.back().back() ^= 0x01;
        } else if (!spend.vin[0].scriptSig.empty()) {
            *spend.vin[0].scriptSig.rbegin() ^= 0x01;
        }
        CBlock block = CreateBlock({spend}, dest, chainstate);
        std::string reason;
        BOOST_CHECK_MESSAGE(DryReject(chainman, chainstate, block, reason),
                            "predicate: " + reason);
        ProcessForce(chainman, block);
        ExpectUnchanged(before, chainman, "invalid_predicate");
    }
    {
        CMutableTransaction spend = CreateValidMempoolTransaction(
            m_coinbase_txns.front(), 0, 1, coinbaseKey, dest, 50 * COIN, false);
        CBlock block = CreateBlock({spend, spend}, dest, chainstate);
        std::string reason;
        BOOST_CHECK_MESSAGE(DryReject(chainman, chainstate, block, reason),
                            "duplicate: " + reason);
        ProcessForce(chainman, block);
        ExpectUnchanged(before, chainman, "duplicate_tx");
    }
}

BOOST_AUTO_TEST_CASE(connect_disconnect_connect_restores_utxo)
{
    ChainstateManager& chainman = *Assert(m_node.chainman);
    Chainstate& chainstate = chainman.ActiveChainstate();
    const CScript dest = PayoutScript(coinbaseKey);
    const CBlock added = CreateAndProcessBlock({}, dest);
    BOOST_REQUIRE(!added.vtx.empty());
    const COutPoint coinbase_out(added.vtx[0]->GetHash(), 0);

    uint256 connected_hash;
    int connected_height = 0;
    {
        LOCK(cs_main);
        connected_hash = chainman.ActiveChain().Tip()->GetBlockHash();
        connected_height = chainman.ActiveChain().Height();
        BOOST_REQUIRE(chainstate.CoinsTip().HaveCoin(coinbase_out));
        BOOST_CHECK_EQUAL(chainstate.CoinsTip().GetBestBlock(), connected_hash);
    }

    BlockValidationState inv_state;
    CBlockIndex* tip = WITH_LOCK(cs_main, return chainman.ActiveChain().Tip());
    BOOST_REQUIRE(chainstate.InvalidateBlock(inv_state, tip));
    {
        LOCK(cs_main);
        BOOST_CHECK_EQUAL(chainman.ActiveChain().Height(), connected_height - 1);
        BOOST_CHECK(chainman.ActiveChain().Tip()->GetBlockHash() != connected_hash);
        BOOST_CHECK(!chainstate.CoinsTip().HaveCoin(coinbase_out));
    }

    {
        LOCK(cs_main);
        chainstate.ResetBlockFailureFlags(tip);
    }
    BlockValidationState act_state;
    BOOST_REQUIRE(chainstate.ActivateBestChain(act_state));
    {
        LOCK(cs_main);
        BOOST_CHECK_EQUAL(chainman.ActiveChain().Height(), connected_height);
        BOOST_CHECK_EQUAL(chainman.ActiveChain().Tip()->GetBlockHash(), connected_hash);
        BOOST_CHECK(chainstate.CoinsTip().HaveCoin(coinbase_out));
        BOOST_CHECK_EQUAL(chainstate.CoinsTip().GetBestBlock(), connected_hash);
    }
}

BOOST_AUTO_TEST_SUITE_END()
