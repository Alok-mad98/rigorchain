// Copyright (c) 2016-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <key.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/transaction_utils.h>

#include <array>

// Microbenchmark for verification of a basic P2WPKH script. Can be easily
// modified to measure performance of other types of scripts.
static void VerifyScriptBench(benchmark::Bench& bench)
{
    ECC_Start();

    const uint32_t flags{SCRIPT_VERIFY_WITNESS};
    const int witnessversion = 0;

    // Key pair.
    CKey key;
    static const std::array<unsigned char, 32> vchKey = {
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
        }
    };
    key.Set(vchKey.begin(), vchKey.end(), false);
    CPubKey pubkey = key.GetPubKey();
    const uint160 pubkeyHash = pubkey.GetEd25519KeyHash();

    // Script.
    CScript scriptPubKey = CScript() << witnessversion << ToByteVector(pubkeyHash);
    CScript scriptSig;
    const CMutableTransaction& txCredit = BuildCreditingTransaction(scriptPubKey, 1);
    CMutableTransaction txSpend = BuildSpendingTransaction(scriptSig, CScriptWitness(), CTransaction(txCredit));
    CScriptWitness& witness = txSpend.vin[0].scriptWitness;
    witness.stack.emplace_back();
    PrecomputedTransactionData txdata(txSpend);
    key.Sign(SignatureHash(scriptPubKey, txSpend, 0, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, uint256::ZERO, &txdata), witness.stack.back());
    witness.stack.push_back(pubkey.GetEd25519Payload());

    // Benchmark.
    bench.run([&] {
        ScriptError err;
        bool success = VerifyRigorPredicate(
            txSpend.vin[0].scriptSig,
            txCredit.vout[0].scriptPubKey,
            &txSpend.vin[0].scriptWitness,
            flags,
            MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, uint256::ZERO),
            &err);
        assert(err == SCRIPT_ERR_OK);
        assert(success);
    });
    ECC_Stop();
}

static void VerifyNestedIfScript(benchmark::Bench& bench)
{
    std::vector<std::vector<unsigned char>> stack;
    CScript script;
    for (int i = 0; i < 100; ++i) {
        script << RIGOR_PUSH_1 << RIGOR_RESERVED_LEGACY_IF;
    }
    for (int i = 0; i < 1000; ++i) {
        script << RIGOR_PUSH_1;
    }
    for (int i = 0; i < 100; ++i) {
        script << RIGOR_RESERVED_LEGACY_ENDIF;
    }
    bench.run([&] {
        auto stack_copy = stack;
        (void)script;
        bool ret = EvalPushOnlyScript(CScript() << RIGOR_PUSH_1 << RIGOR_PUSH_1, stack_copy);
        assert(ret);
    });
}

BENCHMARK(VerifyScriptBench, benchmark::PriorityLevel::HIGH);
BENCHMARK(VerifyNestedIfScript, benchmark::PriorityLevel::HIGH);
