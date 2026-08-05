// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/convergence_commitment.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergenceres.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <crypto/ed25519_provider.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <policy/policy.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <test/util/transaction_utils.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>
#include <cassert>
#include <cstring>

namespace {
/** OP_1 + 32-byte witness v1 program: ASCII ORB2 + pubkey[4..31] (matches interpreter spend binding). */
CScript MakeOrb2ScriptPubKeyForPubkey(const std::vector<unsigned char>& pubkey)
{
    assert(pubkey.size() == ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> wp(32);
    std::memcpy(wp.data(), RigorTags::Res2WirePrefix.data(), RigorTags::Res2WirePrefix.size());
    std::memcpy(wp.data() + 4, pubkey.data() + 4, 28);
    return CScript() << OP_1 << wp;
}

/** Valid ORB2-tagged output for consensus classification (prefix only; tail arbitrary). */
CScript MakeOrb2ScriptPubKeyMinimal()
{
    std::vector<unsigned char> wp(32, 0xab);
    std::memcpy(wp.data(), RigorTags::Res2WirePrefix.data(), RigorTags::Res2WirePrefix.size());
    return CScript() << OP_1 << wp;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(rigor_witness_tx_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(witness_v0_p2wpkh_ed25519_verify_script_ok)
{
    const std::vector<unsigned char> seed = ParseHex("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");
    std::vector<unsigned char> pubkey(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pubkey, seckey));

    const uint160 keyhash = Hash160(pubkey);
    const CScript scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(keyhash));

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);

    CScript scriptCode;
    scriptCode << OP_DUP << OP_HASH160 << ToByteVector(keyhash) << OP_EQUALVERIFY << OP_CHECKSIG;

    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptCode, txSpend, 0, SIGHASH_ALL, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, &txdata);

    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE + 1);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));
    witSig[ed25519::SIGNATURE_SIZE] = SIGHASH_ALL;

    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pubkey);
    txSpend.vin[0].scriptWitness = witness;

    const unsigned int flags{STANDARD_SCRIPT_VERIFY_FLAGS};
    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyScript(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        flags,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL),
        &err);
    BOOST_CHECK_MESSAGE(ok, ScriptErrorString(err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(witness_v1_res_ed25519_verify_ok)
{
    const std::vector<unsigned char> seed = ParseHex("ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100");
    std::vector<unsigned char> pubkey(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pubkey, seckey));

    const CScript scriptPubKey = MakeOrb2ScriptPubKeyForPubkey(pubkey);

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);

    CScript scriptCode;
    scriptCode << OP_DUP << OP_HASH160 << ToByteVector(uint160(Hash160(pubkey))) << OP_EQUALVERIFY << OP_CHECKSIG;

    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptCode, txSpend, 0, SIGHASH_ALL, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, &txdata);

    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE + 1);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));
    witSig[ed25519::SIGNATURE_SIZE] = SIGHASH_ALL;

    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pubkey);
    txSpend.vin[0].scriptWitness = witness;

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyScript(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL),
        &err);
    BOOST_CHECK_MESSAGE(ok, ScriptErrorString(err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(witness_v1_res_ecdsa_sig_fails)
{
    const std::vector<unsigned char> seed = ParseHex("1111111111111111111111111111111111111111111111111111111111111111");
    std::vector<unsigned char> pubkey(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pubkey, seckey));

    const CScript scriptPubKey = MakeOrb2ScriptPubKeyForPubkey(pubkey);
    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);

    // Legacy ECDSA-shaped witness (DER-ish + hashtype) — wrong size for Ed25519; must not verify.
    std::vector<unsigned char> der_ecdsa(71, 0x30);
    der_ecdsa.push_back(static_cast<unsigned char>(SIGHASH_ALL));

    CScriptWitness witness;
    witness.stack.push_back(der_ecdsa);
    witness.stack.push_back(pubkey);
    txSpend.vin[0].scriptWitness = witness;

    PrecomputedTransactionData txdata(txSpend);
    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyScript(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL),
        &err);
    BOOST_CHECK(!ok);
    BOOST_CHECK(err == SCRIPT_ERR_SIG_DER);
}

/** OP_0 + 32-byte program: when SHA256(last_witness) != program, spend as native Ed25519 P2WPKH (program = pubkey). */
BOOST_AUTO_TEST_CASE(witness_v0_32byte_program_native_ed25519_ok)
{
    // Same 32-byte seed as witness_v0_p2wpkh_ed25519_verify_script_ok (known-good ParseHex + libsodium).
    const std::vector<unsigned char> seed = ParseHex("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");
    BOOST_REQUIRE_EQUAL(seed.size(), ed25519::SEED_SIZE);
    std::vector<unsigned char> pubkey(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pubkey, seckey));

    const CScript scriptPubKey = CScript() << OP_0 << pubkey;

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);

    CScript scriptCode;
    scriptCode << OP_DUP << OP_HASH160 << ToByteVector(uint160(Hash160(pubkey))) << OP_EQUALVERIFY << OP_CHECKSIG;

    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptCode, txSpend, 0, SIGHASH_ALL, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, &txdata);

    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE + 1);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));
    witSig[ed25519::SIGNATURE_SIZE] = SIGHASH_ALL;

    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pubkey);
    txSpend.vin[0].scriptWitness = witness;

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyScript(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL),
        &err);
    BOOST_CHECK_MESSAGE(ok, ScriptErrorString(err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

/** 32-byte v0 program with SHA256(witness_script)==program must still run as P2WSH (not native pubkey). */
BOOST_AUTO_TEST_CASE(witness_v0_32byte_p2wsh_takes_priority)
{
    CScript wit_script;
    wit_script << OP_1;
    uint256 program;
    CSHA256().Write(wit_script.data(), wit_script.size()).Finalize(program.begin());
    const CScript scriptPubKey = CScript() << OP_0 << ToByteVector(program);

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);

    CScriptWitness witness;
    witness.stack.emplace_back(wit_script.begin(), wit_script.end());
    txSpend.vin[0].scriptWitness = witness;

    PrecomputedTransactionData txdata(txSpend);
    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyScript(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL),
        &err);
    BOOST_CHECK_MESSAGE(ok, ScriptErrorString(err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(oru_convergence_tx_passes_checktxinputs)
{
    const CScript oru_spk = CScript() << OP_0 << std::vector<unsigned char>(20, 0x44);
    const CAmount burn = COIN;

    CCoinsView base;
    CCoinsViewCache coins(&base);
    CTxOut prev_origo(20 * COIN, oru_spk);
    const uint256 h_origo{uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")};
    coins.AddCoin(COutPoint(h_origo, 0), Coin(CTxOut{prev_origo}, 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(h_origo, 0);
    mtx.vout.resize(2);
    mtx.vout[0].nValue = burn;
    mtx.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
    mtx.vout[1].nValue = 19 * COIN;
    mtx.vout[1].scriptPubKey = oru_spk;

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(fee, 0);
}

BOOST_AUTO_TEST_CASE(res_input_tx_with_convergence_output_rejected)
{
    const CScript oru_spk = CScript() << OP_0 << std::vector<unsigned char>(20, 0x55);
    const CScript res_spk = MakeOrb2ScriptPubKeyMinimal();

    CCoinsView base;
    CCoinsViewCache coins(&base);
    CTxOut prev_res(50 * RES_COIN, res_spk);
    const uint256 h_res{uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")};
    coins.AddCoin(COutPoint(h_res, 0), Coin(CTxOut{prev_res}, 100, false), false);

    const CAmount burn = 10 * RES_COIN;

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(h_res, 0);
    mtx.vout.resize(2);
    mtx.vout[0].nValue = burn;
    mtx.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
    mtx.vout[1].nValue = 40 * RES_COIN - burn;
    mtx.vout[1].scriptPubKey = res_spk;

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "convergence-must-be-orum");
}

BOOST_AUTO_TEST_CASE(oru_convergence_two_convergence_outputs_rejected)
{
    const CScript oru_spk = CScript() << OP_0 << std::vector<unsigned char>(20, 0x66);

    CCoinsView base;
    CCoinsViewCache coins(&base);
    CTxOut prev_origo(30 * COIN, oru_spk);
    const uint256 h_origo{uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc")};
    coins.AddCoin(COutPoint(h_origo, 0), Coin(CTxOut{prev_origo}, 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(h_origo, 0);
    mtx.vout.resize(3);
    mtx.vout[0].nValue = COIN;
    mtx.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
    mtx.vout[1].nValue = COIN;
    mtx.vout[1].scriptPubKey = Consensus::ConvergenceOutputScript();
    mtx.vout[2].nValue = 28 * COIN;
    mtx.vout[2].scriptPubKey = oru_spk;

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "invalid-convergence-structure");
}

BOOST_AUTO_TEST_SUITE_END()
