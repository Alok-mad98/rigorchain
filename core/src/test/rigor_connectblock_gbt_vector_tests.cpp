// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/rigor_connectblock_vectors.json.h>
#include <test/data/rigor_gbt_layout_vectors.json.h>

#include <chain.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <hash.h>
#include <key.h>
#include <node/context.h>
#include <node/miner.h>
#include <pow.h>
#include <pow_opus.h>
#include <primitives/block.h>
#include <pubkey.h>
#include <rigor_kernel/pow_digest.h>
#include <script/predicate_verify.h>
#include <script/standard.h>
#include <streams.h>
#include <test/util/json.h>
#include <test/util/setup_common.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <memory>
#include <string>
#include <vector>

using node::BlockAssembler;

BOOST_FIXTURE_TEST_SUITE(rigor_connectblock_gbt_vector_tests, RegTestingSetup)

static CKey FrozenCoinbaseKey()
{
    constexpr std::array<unsigned char, 32> vchKey = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
    CKey key;
    key.Set(vchKey.begin(), vchKey.end(), true);
    BOOST_REQUIRE(key.IsValid());
    return key;
}

static CScript FrozenPayoutScript()
{
    return GetScriptForDestination(WitnessV0KeyHash(FrozenCoinbaseKey().GetPubKey()));
}

static void FreezeClock()
{
    const int64_t genesis_time = Params().GenesisBlock().nTime;
    SetMockTime(std::chrono::seconds{genesis_time + 100});
}

static std::string EncodeHexBlk(const CBlock& block)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << block;
    return HexStr(ss);
}

static CBlock MakeFrozenHeight1Block(const node::NodeContext& node)
{
    FreezeClock();
    const CScript spk = FrozenPayoutScript();
    auto tmpl = BlockAssembler{Assert(node.chainman)->ActiveChainstate(), Assert(node.mempool.get())}
                    .CreateNewBlock(spk);
    BOOST_REQUIRE(tmpl);
    CBlock block = tmpl->block;
    block.hashMerkleRoot = BlockMerkleRoot(block);
    while (!CheckProofOfWork(block, block.nBits, Params().GetConsensus())) {
        ++block.nNonce;
        BOOST_REQUIRE(block.nNonce != 0);
    }
    return block;
}

BOOST_AUTO_TEST_CASE(gbt_layout_vectors_p0_016)
{
    FreezeClock();
    UniValue root = read_json(std::string(
        json_tests::rigor_gbt_layout_vectors,
        json_tests::rigor_gbt_layout_vectors + sizeof(json_tests::rigor_gbt_layout_vectors)));
    BOOST_REQUIRE(root.isArray());
    BOOST_REQUIRE_EQUAL(root.size(), 1U);
    const UniValue& v = root[0];

    const CScript spk = FrozenPayoutScript();
    auto tmpl = BlockAssembler{Assert(m_node.chainman)->ActiveChainstate(), Assert(m_node.mempool.get())}
                    .CreateNewBlock(spk);
    BOOST_REQUIRE(tmpl);
    const CBlock& block = tmpl->block;
    BOOST_REQUIRE(!block.vtx.empty());
    const CTransaction& cb = *block.vtx[0];

    BOOST_CHECK_EQUAL(v["height"].getInt<int>(), 1);
    BOOST_CHECK_EQUAL(cb.vout[0].nValue, v["coinbasevalue"].getInt<CAmount>());
    BOOST_CHECK_EQUAL(HexStr(spk), v["payout_scriptPubKey"].get_str());
    BOOST_CHECK_EQUAL(GetWitnessCommitmentIndex(block), v["witness_commitment_index"].getInt<int>());
    BOOST_CHECK_EQUAL(HexStr(tmpl->vchCoinbaseCommitment), v["default_witness_commitment"].get_str());
    BOOST_CHECK_EQUAL(HexStr(cb.vin[0].scriptSig), v["coinbase_scriptSig"].get_str());
    BOOST_CHECK_EQUAL(BlockMerkleRoot(block).GetHex(), v["merkle_root"].get_str());
    BOOST_CHECK_EQUAL(block.hashPrevBlock.GetHex(), v["prevhash"].get_str());
    BOOST_CHECK_EQUAL(strprintf("%08x", block.nBits), v["bits"].get_str());
    BOOST_CHECK_EQUAL(block.nTime, v["nTime"].getInt<int64_t>());
    BOOST_CHECK_EQUAL(static_cast<int>(cb.vout.size()), 2);

    bool mint_present = false;
    std::string mint_hex;
    for (const CTxOut& out : cb.vout) {
        if (IsResMintScript(out.scriptPubKey)) {
            mint_present = true;
            mint_hex = HexStr(out.scriptPubKey);
        }
    }
    BOOST_CHECK_EQUAL(mint_present, v["mint_present"].get_bool());
    BOOST_CHECK_EQUAL(mint_hex, v["mint_scriptPubKey"].get_str());

    const CPubKey pk = FrozenCoinbaseKey().GetPubKey();
    const uint160 pkh = Hash160(pk);
    const CScript canonical = GetResMintScript(v["canonical_mint_maturity_height"].getInt<int64_t>(), pkh);
    BOOST_CHECK_EQUAL(HexStr(canonical), v["canonical_mint_script"].get_str());
}

BOOST_AUTO_TEST_CASE(connectblock_valid_height1_hex)
{
    UniValue root = read_json(std::string(
        json_tests::rigor_connectblock_vectors,
        json_tests::rigor_connectblock_vectors + sizeof(json_tests::rigor_connectblock_vectors)));
    BOOST_REQUIRE(root.isArray());
    BOOST_REQUIRE_EQUAL(root.size(), 2U);

    const CBlock live = MakeFrozenHeight1Block(m_node);
    const std::string live_hex = EncodeHexBlk(live);
    const std::string expected = root[0]["block_hex"].get_str();
    BOOST_CHECK_MESSAGE(live_hex == expected, "live_hex=" + live_hex);

    CBlock decoded;
    BOOST_REQUIRE_MESSAGE(DecodeHexBlk(decoded, expected), "DecodeHexBlk valid");
    const auto shared = std::make_shared<const CBlock>(decoded);
    BOOST_REQUIRE(Assert(m_node.chainman)->ProcessNewBlock(shared, true, true, nullptr));
    BOOST_CHECK_EQUAL(WITH_LOCK(::cs_main, return m_node.chainman->ActiveChain().Height()), 1);
}

BOOST_AUTO_TEST_CASE(connectblock_invalid_stale_merkle_hex)
{
    UniValue root = read_json(std::string(
        json_tests::rigor_connectblock_vectors,
        json_tests::rigor_connectblock_vectors + sizeof(json_tests::rigor_connectblock_vectors)));
    const CBlock live = MakeFrozenHeight1Block(m_node);
    CBlock mutated = live;
    CMutableTransaction junk;
    junk.nVersion = 2;
    junk.vin.resize(1);
    junk.vout.resize(1);
    junk.vout[0].nValue = 1;
    junk.vout[0].scriptPubKey = FrozenPayoutScript();
    mutated.vtx.push_back(MakeTransactionRef(junk));
    const std::string live_invalid = EncodeHexBlk(mutated);
    const std::string expected = root[1]["block_hex"].get_str();
    BOOST_CHECK_MESSAGE(live_invalid == expected, "live_invalid_hex=" + live_invalid);

    CBlock decoded;
    BOOST_REQUIRE_MESSAGE(DecodeHexBlk(decoded, expected), "DecodeHexBlk invalid");
    const auto shared = std::make_shared<const CBlock>(decoded);
    BOOST_CHECK(!Assert(m_node.chainman)->ProcessNewBlock(shared, true, true, nullptr));
    BOOST_CHECK_EQUAL(WITH_LOCK(::cs_main, return m_node.chainman->ActiveChain().Height()), 0);
}

static CTransactionRef DummyTx(const CScript& spk, CAmount value)
{
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vout[0].nValue = value;
    tx.vout[0].scriptPubKey = spk;
    return MakeTransactionRef(tx);
}

static void ExpectProcessReject(node::NodeContext& node, const CBlock& mutated, const std::string& name)
{
    const auto shared = std::make_shared<const CBlock>(mutated);
    BOOST_CHECK_MESSAGE(!Assert(node.chainman)->ProcessNewBlock(shared, true, true, nullptr),
                        name + " must not ProcessNewBlock-accept");
    BOOST_CHECK_EQUAL(WITH_LOCK(::cs_main, return node.chainman->ActiveChain().Height()), 0);
}

BOOST_AUTO_TEST_CASE(connectblock_consensus_mutations_p0_085)
{
    UniValue root = read_json(std::string(
        json_tests::rigor_connectblock_vectors,
        json_tests::rigor_connectblock_vectors + sizeof(json_tests::rigor_connectblock_vectors)));
    CBlock valid;
    BOOST_REQUIRE(DecodeHexBlk(valid, root[0]["block_hex"].get_str()));
    const std::string original = EncodeHexBlk(valid);
    const CScript spk = FrozenPayoutScript();

    {
        CBlock m = valid;
        *m.hashMerkleRoot.begin() ^= 0x01;
        BOOST_CHECK(EncodeHexBlk(m) != original);
        ExpectProcessReject(m_node, m, "merkle_root_mutation");
    }
    {
        CBlock m = valid;
        CMutableTransaction cb(*m.vtx[0]);
        cb.vout[0].nValue += 1;
        m.vtx[0] = MakeTransactionRef(cb);
        BOOST_CHECK(EncodeHexBlk(m) != original);
        ExpectProcessReject(m_node, m, "coinbase_mutation");
    }
    {
        const auto tx1 = DummyTx(spk, 1);
        const auto tx2 = DummyTx(spk, 2);
        CBlock swapped = valid;
        swapped.vtx.push_back(tx2);
        swapped.vtx.push_back(tx1);
        CBlock m = valid;
        m.vtx.push_back(tx1);
        m.vtx.push_back(tx2);
        m.hashMerkleRoot = BlockMerkleRoot(swapped);
        BOOST_CHECK(EncodeHexBlk(m) != original);
        ExpectProcessReject(m_node, m, "tx_ordering_mutation");
    }
    {
        CBlock m = valid;
        const int wpos = GetWitnessCommitmentIndex(m);
        BOOST_REQUIRE_GE(wpos, 0);
        CMutableTransaction cb(*m.vtx[0]);
        BOOST_REQUIRE(!cb.vout[wpos].scriptPubKey.empty());
        cb.vout[wpos].scriptPubKey[cb.vout[wpos].scriptPubKey.size() - 1] ^= 0x01;
        m.vtx[0] = MakeTransactionRef(cb);
        BOOST_CHECK(EncodeHexBlk(m) != original);
        ExpectProcessReject(m_node, m, "witness_commitment_mutation");
    }
    {
        CBlock m = valid;
        m.nBits = 0;
        BOOST_CHECK(EncodeHexBlk(m) != original);
        ExpectProcessReject(m_node, m, "nBits_mutation");
    }
    {
        CBlock m = valid;
        m.nTime += 1;
        BOOST_CHECK(EncodeHexBlk(m) != original);
        BOOST_CHECK(m.GetHash() != valid.GetHash());
        BOOST_CHECK(rigor_kernel::GetRigorPowHash(m) != rigor_kernel::GetRigorPowHash(valid));
        if (!CheckProofOfWork(m, m.nBits, Params().GetConsensus())) {
            ExpectProcessReject(m_node, m, "header_timestamp_mutation");
        }
    }
    {
        CBlock m = valid;
        ++m.nNonce;
        BOOST_CHECK(EncodeHexBlk(m) != original);
        BOOST_CHECK(m.GetHash() != valid.GetHash());
        BOOST_CHECK(rigor_kernel::GetRigorPowHash(m) != rigor_kernel::GetRigorPowHash(valid));
        if (!CheckProofOfWork(m, m.nBits, Params().GetConsensus())) {
            ExpectProcessReject(m_node, m, "nonce_mutation");
        }
    }
}

BOOST_AUTO_TEST_CASE(pow_hash_not_legacy_opuhash_p0_085)
{
    UniValue root = read_json(std::string(
        json_tests::rigor_connectblock_vectors,
        json_tests::rigor_connectblock_vectors + sizeof(json_tests::rigor_connectblock_vectors)));
    CBlock valid;
    BOOST_REQUIRE(DecodeHexBlk(valid, root[0]["block_hex"].get_str()));
    const uint256 sha3 = rigor_kernel::GetRigorPowHash(valid);
    const uint256 opus = GetOpusHash(valid);
    BOOST_CHECK(sha3 != opus);
    BOOST_CHECK(CheckProofOfWork(valid, valid.nBits, Params().GetConsensus()));
}

BOOST_AUTO_TEST_SUITE_END()
