// Copyright (c) 2026 The RIGOR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/sigcache.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <test/util/transaction_utils.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>
#include <cstring>

BOOST_FIXTURE_TEST_SUITE(ed25519_consensus_determinism_tests, BasicTestingSetup)

namespace {

struct SignedSpend {
    CTransaction credit;
    CMutableTransaction spend;
    PrecomputedTransactionData txdata;
};

SignedSpend MakeValidOruSpend()
{
    const std::vector<unsigned char> seed = ParseHex("deadbeefcafebabe0123456789abcdefdeadbeefcafebabe0123456789abcdef");
    std::vector<unsigned char> pk32(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pk32, seckey));

    const uint160 keyhash = Hash160(pk32);
    const CScript scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(keyhash));
    CTransaction credit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction spend = BuildSpendingTransaction(CScript(), CScriptWitness(), credit);

    CScript scriptCode;
    scriptCode << OP_DUP << OP_HASH160 << ToByteVector(keyhash) << OP_EQUALVERIFY << OP_CHECKSIG;
    PrecomputedTransactionData txdata(spend);
    const uint256 sighash = SignatureHash(scriptCode, spend, 0, SIGHASH_ALL, credit.vout[0].nValue, SigVersion::WITNESS_V0, &txdata);

    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE + 1);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));
    witSig[ed25519::SIGNATURE_SIZE] = SIGHASH_ALL;
    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pk32);
    spend.vin[0].scriptWitness = witness;

    return SignedSpend{std::move(credit), std::move(spend), std::move(txdata)};
}

} // namespace

BOOST_AUTO_TEST_CASE(same_tx_two_checkers_same_result)
{
    SignedSpend s = MakeValidOruSpend();

    ScriptError err_a{SCRIPT_ERR_UNKNOWN_ERROR};
    ScriptError err_b{SCRIPT_ERR_UNKNOWN_ERROR};
    MutableTransactionSignatureChecker checker_a(&s.spend, 0, s.credit.vout[0].nValue, s.txdata, MissingDataBehavior::ASSERT_FAIL);
    MutableTransactionSignatureChecker checker_b(&s.spend, 0, s.credit.vout[0].nValue, s.txdata, MissingDataBehavior::ASSERT_FAIL);

    const bool ok_a = VerifyScript(s.spend.vin[0].scriptSig, s.credit.vout[0].scriptPubKey,
        &s.spend.vin[0].scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, checker_a, &err_a);
    const bool ok_b = VerifyScript(s.spend.vin[0].scriptSig, s.credit.vout[0].scriptPubKey,
        &s.spend.vin[0].scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, checker_b, &err_b);

    BOOST_CHECK(ok_a);
    BOOST_CHECK(ok_b);
    BOOST_CHECK_EQUAL(err_a, err_b);
    BOOST_CHECK_EQUAL(err_a, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(same_tx_caching_checker_repeatable)
{
    SignedSpend s = MakeValidOruSpend();
    const CTransaction spend_tx{s.spend};
    PrecomputedTransactionData txdata(spend_tx);

    ScriptError err1{SCRIPT_ERR_UNKNOWN_ERROR};
    ScriptError err2{SCRIPT_ERR_UNKNOWN_ERROR};
    CachingTransactionSignatureChecker cache_a(&spend_tx, 0, s.credit.vout[0].nValue, /*store=*/true, txdata);
    CachingTransactionSignatureChecker cache_b(&spend_tx, 0, s.credit.vout[0].nValue, /*store=*/true, txdata);

    const bool ok1 = VerifyScript(spend_tx.vin[0].scriptSig, s.credit.vout[0].scriptPubKey,
        &spend_tx.vin[0].scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, cache_a, &err1);
    const bool ok2 = VerifyScript(spend_tx.vin[0].scriptSig, s.credit.vout[0].scriptPubKey,
        &spend_tx.vin[0].scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, cache_b, &err2);

    BOOST_CHECK(ok1);
    BOOST_CHECK(ok2);
    BOOST_CHECK_EQUAL(err1, err2);
}

BOOST_AUTO_TEST_CASE(mandatory_and_standard_flags_same_crypto_outcome)
{
    SignedSpend s = MakeValidOruSpend();
    MutableTransactionSignatureChecker checker(&s.spend, 0, s.credit.vout[0].nValue, s.txdata, MissingDataBehavior::ASSERT_FAIL);

    ScriptError err_std{SCRIPT_ERR_UNKNOWN_ERROR};
    ScriptError err_min{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok_std = VerifyScript(s.spend.vin[0].scriptSig, s.credit.vout[0].scriptPubKey,
        &s.spend.vin[0].scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, checker, &err_std);
    const bool ok_min = VerifyScript(s.spend.vin[0].scriptSig, s.credit.vout[0].scriptPubKey,
        &s.spend.vin[0].scriptWitness, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS, checker, &err_min);

    BOOST_CHECK(ok_std);
    BOOST_CHECK(ok_min);
    BOOST_CHECK_EQUAL(err_std, SCRIPT_ERR_OK);
    BOOST_CHECK_EQUAL(err_min, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_SUITE_END()
