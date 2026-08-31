// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <coins.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <key.h>
#include <node/miner.h>
#include <script/predicate_verify.h>
#include <script/standard.h>
#include <test/util/mining.h>
#include <test/util/script.h>
#include <test/util/setup_common.h>
#include <txmempool.h>
#include <validation.h>


#include <array>
#include <vector>

static CKey BenchP2wpkhKey()
{
    CKey key;
    const std::array<unsigned char, 32> vchKey = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
    key.Set(vchKey.begin(), vchKey.end(), true);
    return key;
}

static CScript BenchP2wpkhScript(const CKey& key)
{
    return GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey()));
}

static void SignBenchP2wpkhInput(CMutableTransaction& tx, const CKey& key, CAmount amount)
{
    const CPubKey pubkey = key.GetPubKey();
    const CScript scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(pubkey));
    PrecomputedTransactionData txdata(tx);
    const uint256 sighash = SignatureHash(scriptPubKey, tx, 0, amount, SigVersion::WITNESS_V0, TestSighashChainId(), &txdata);
    std::vector<unsigned char> witSig;
    assert(key.Sign(sighash, witSig));
    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pubkey.GetEd25519Payload());
    tx.vin[0].scriptWitness = witness;
}

static void AssembleBlock(benchmark::Bench& bench)
{
    const auto test_setup = MakeNoLogFileContext<const TestingSetup>();
    const CKey key = BenchP2wpkhKey();
    const CScript spk = BenchP2wpkhScript(key);

    // Collect some loose transactions that spend the coinbases of our mined blocks
    constexpr size_t NUM_BLOCKS{200};
    std::array<CTransactionRef, NUM_BLOCKS - COINBASE_MATURITY + 1> txs;
    for (size_t b{0}; b < NUM_BLOCKS; ++b) {
        CMutableTransaction tx;
        tx.vin.push_back(MineBlock(test_setup->m_node, spk));
        CAmount amount{0};
        {
            LOCK(::cs_main);
            Coin coin;
            assert(test_setup->m_node.chainman->ActiveChainstate().CoinsTip().GetCoin(tx.vin[0].prevout, coin));
            amount = coin.out.nValue;
        }
        tx.vout.emplace_back(1337, spk);
        SignBenchP2wpkhInput(tx, key, amount);
        if (NUM_BLOCKS - b >= COINBASE_MATURITY)
            txs.at(b) = MakeTransactionRef(tx);
    }
    {
        LOCK(::cs_main);

        for (const auto& txr : txs) {
            const MempoolAcceptResult res = test_setup->m_node.chainman->ProcessTransaction(txr);
            assert(res.m_result_type == MempoolAcceptResult::ResultType::VALID);
        }
    }

    bench.run([&] {
        PrepareBlock(test_setup->m_node, spk);
    });
}
static void BlockAssemblerAddPackageTxns(benchmark::Bench& bench)
{
    FastRandomContext det_rand{true};
    auto testing_setup{MakeNoLogFileContext<TestChain100Setup>()};
    testing_setup->PopulateMempool(det_rand, /*num_transactions=*/1000, /*submit=*/true);
    node::BlockAssembler::Options assembler_options;
    assembler_options.test_block_validity = false;

    bench.run([&] {
        PrepareBlock(testing_setup->m_node, BenchP2wpkhScript(BenchP2wpkhKey()), assembler_options);
    });
}

BENCHMARK(AssembleBlock, benchmark::PriorityLevel::HIGH);
BENCHMARK(BlockAssemblerAddPackageTxns, benchmark::PriorityLevel::LOW);
