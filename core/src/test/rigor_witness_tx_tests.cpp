// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/convergenceres.h>
#include <consensus/tx_check.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <crypto/ed25519_provider.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <policy/policy.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/descriptor.h>
#include <script/predicate_verify.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/standard.h>
#include <test/util/script.h>
#include <test/util/setup_common.h>
#include <test/util/transaction_utils.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>
#include <cstring>
#include <utility>

namespace {
/** Native witness v1 RES (32-byte program, not CVG2 / RES2). */
CScript MakeNativeResScriptPubKeyMinimal()
{
    return TestNativeResScript();
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
    scriptCode << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(keyhash) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;

    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptPubKey, txSpend, 0, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, TestSighashChainId(), &txdata);

    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));

    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pubkey);
    txSpend.vin[0].scriptWitness = witness;

    const unsigned int flags{STANDARD_SCRIPT_VERIFY_FLAGS};
    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyRigorPredicate(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        flags,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()),
        &err);
    BOOST_CHECK_MESSAGE(ok, ScriptErrorString(err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(witness_v0_p2wpkh_65byte_hashtype_suffix_rejected)
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
    scriptCode << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(keyhash) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptPubKey, txSpend, 0, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, TestSighashChainId(), &txdata);

    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));
    witSig.push_back(static_cast<unsigned char>(SIGHASH_ALL));
    BOOST_REQUIRE_EQUAL(witSig.size(), ed25519::SIGNATURE_SIZE + 1);

    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pubkey);
    txSpend.vin[0].scriptWitness = witness;

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyRigorPredicate(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()),
        &err);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_SIG_DER);
}

BOOST_AUTO_TEST_CASE(witness_v0_p2wpkh_sig_length_only_64_accepted)
{
    const std::vector<unsigned char> seed = ParseHex("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");
    std::vector<unsigned char> pubkey(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pubkey, seckey));

    const uint160 keyhash = Hash160(pubkey);
    const CScript scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(keyhash));
    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);
    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptPubKey, txSpend, 0, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, TestSighashChainId(), &txdata);
    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));

    auto check_len = [&](size_t n) {
        std::vector<unsigned char> sig = witSig;
        sig.resize(n, 0x01);
        CScriptWitness witness;
        witness.stack.push_back(sig);
        witness.stack.push_back(pubkey);
        txSpend.vin[0].scriptWitness = witness;
        ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
        const bool ok = VerifyRigorPredicate(
            txSpend.vin[0].scriptSig,
            txCredit.vout[0].scriptPubKey,
            &txSpend.vin[0].scriptWitness,
            STANDARD_SCRIPT_VERIFY_FLAGS,
            MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()),
            &err);
        return std::pair<bool, ScriptError>{ok, err};
    };

    BOOST_CHECK(!check_len(63).first);
    BOOST_CHECK(!check_len(65).first);
    BOOST_CHECK(!check_len(66).first);
    const auto ok64 = check_len(64);
    BOOST_CHECK_MESSAGE(ok64.first, ScriptErrorString(ok64.second));
}

BOOST_AUTO_TEST_CASE(checkmultisig_is_not_a_consensus_lock)
{
    ScriptError err{SCRIPT_ERR_OK};
    const CScript script = CScript() << RIGOR_PUSH_0 << RIGOR_PUSH_0 << RIGOR_PUSH_1 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK(!VerifyRigorPredicate(CScript(), script, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, BaseSignatureChecker(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
}

BOOST_AUTO_TEST_CASE(witness_v1_res_ed25519_verify_ok)
{
    const std::vector<unsigned char> seed = ParseHex("ffeeddccbbaa99887766554433221100ffeeddccbbaa99887766554433221100");
    std::vector<unsigned char> pubkey(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pubkey, seckey));

    std::vector<unsigned char> program(32);
    BOOST_REQUIRE(ed25519::ResWitnessProgram(pubkey, program));
    const CScript scriptPubKey = CScript() << RIGOR_PUSH_1 << program;

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);

    CScript scriptCode;
    scriptCode << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(uint160(Hash160(pubkey))) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;

    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptPubKey, txSpend, 0, txCredit.vout[0].nValue, SigVersion::WITNESS_V1_RES, TestSighashChainId(), &txdata);

    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));

    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pubkey);
    txSpend.vin[0].scriptWitness = witness;

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyRigorPredicate(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()),
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

    std::vector<unsigned char> program(32);
    BOOST_REQUIRE(ed25519::ResWitnessProgram(pubkey, program));
    const CScript scriptPubKey = CScript() << RIGOR_PUSH_1 << program;
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
    const bool ok = VerifyRigorPredicate(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()),
        &err);
    BOOST_CHECK(!ok);
    BOOST_CHECK(err == SCRIPT_ERR_SIG_DER);
}

/** RIGOR_PUSH_0 + 32-byte program is P2WSH only (RIG-07): native Ed25519 fallback is consensus-invalid. */
BOOST_AUTO_TEST_CASE(witness_v0_32byte_program_native_ed25519_rejected)
{
    // Same 32-byte seed as witness_v0_p2wpkh_ed25519_verify_script_ok (known-good ParseHex + libsodium).
    const std::vector<unsigned char> seed = ParseHex("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");
    BOOST_REQUIRE_EQUAL(seed.size(), ed25519::SEED_SIZE);
    std::vector<unsigned char> pubkey(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pubkey, seckey));

    const CScript scriptPubKey = CScript() << RIGOR_PUSH_0 << pubkey;

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);

    CScript scriptCode;
    scriptCode << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(uint160(Hash160(pubkey))) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;

    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptPubKey, txSpend, 0, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, TestSighashChainId(), &txdata);

    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));

    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pubkey);
    txSpend.vin[0].scriptWitness = witness;

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyRigorPredicate(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()),
        &err);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH);
}

/** 32-byte v0 program is rejected: P2WSH is not a consensus spend path. */
BOOST_AUTO_TEST_CASE(witness_v0_32byte_p2wsh_takes_priority)
{
    CScript wit_script;
    wit_script << RIGOR_PUSH_1;
    uint256 program;
    CSHA256().Write(wit_script.data(), wit_script.size()).Finalize(program.begin());
    const CScript scriptPubKey = CScript() << RIGOR_PUSH_0 << ToByteVector(program);

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);

    CScriptWitness witness;
    witness.stack.emplace_back(wit_script.begin(), wit_script.end());
    txSpend.vin[0].scriptWitness = witness;

    PrecomputedTransactionData txdata(txSpend);
    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyRigorPredicate(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()),
        &err);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH);
}

BOOST_AUTO_TEST_CASE(oru_convergence_tx_passes_checktxinputs)
{
    const CScript oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x44);
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
    const CScript oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x55);
    const CScript res_spk = MakeNativeResScriptPubKeyMinimal();

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

BOOST_AUTO_TEST_CASE(witness_v0_p2wpkh_oru_wrapper_ed25519_ok)
{
    const std::vector<unsigned char> seed = ParseHex("aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899");
    std::vector<unsigned char> pk32(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pk32, seckey));

    const uint160 keyhash = Hash160(pk32);
    const CScript scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(keyhash));

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);

    CScript scriptCode;
    scriptCode << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(keyhash) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptPubKey, txSpend, 0, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, TestSighashChainId(), &txdata);

    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));

    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pk32);
    txSpend.vin[0].scriptWitness = witness;

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyRigorPredicate(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()),
        &err);
    BOOST_CHECK_MESSAGE(ok, ScriptErrorString(err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(witness_v0_invalid_ed25519_signature_fails)
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
    scriptCode << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(keyhash) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptPubKey, txSpend, 0, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, TestSighashChainId(), &txdata);

    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));
    witSig[0] ^= 0x01;

    CScriptWitness witness;
    witness.stack.push_back(witSig);
    witness.stack.push_back(pubkey);
    txSpend.vin[0].scriptWitness = witness;

    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyRigorPredicate(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()),
        &err);
    BOOST_CHECK(!ok);
    BOOST_CHECK(err == SCRIPT_ERR_EVAL_FALSE || err == SCRIPT_ERR_SIG_NULLFAIL);
}

BOOST_AUTO_TEST_CASE(witness_v0_secp256k1_compressed_pubkey_fails)
{
    std::vector<unsigned char> secp_pk(CPubKey::COMPRESSED_SIZE, 0x11);
    secp_pk[0] = 0x02;
    const uint160 keyhash = Hash160(secp_pk);
    const CScript scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(keyhash));
    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);

    std::vector<unsigned char> der(71, 0x30);
    der.push_back(static_cast<unsigned char>(SIGHASH_ALL));

    CScriptWitness witness;
    witness.stack.push_back(der);
    witness.stack.push_back(secp_pk);
    txSpend.vin[0].scriptWitness = witness;

    PrecomputedTransactionData txdata(txSpend);
    ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
    const bool ok = VerifyRigorPredicate(
        txSpend.vin[0].scriptSig,
        txCredit.vout[0].scriptPubKey,
        &txSpend.vin[0].scriptWitness,
        STANDARD_SCRIPT_VERIFY_FLAGS,
        MutableTransactionSignatureChecker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()),
        &err);
    BOOST_CHECK(!ok);
    BOOST_CHECK(err == SCRIPT_ERR_PUBKEYTYPE || err == SCRIPT_ERR_SECP256K1_DISABLED);
}

BOOST_AUTO_TEST_CASE(check_ed25519_signature_roundtrip)
{
    const std::vector<unsigned char> seed = ParseHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    std::vector<unsigned char> pk32(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pk32, seckey));
    std::vector<unsigned char> wrapped(CPubKey::COMPRESSED_SIZE);
    wrapped[0] = 0x00;
    std::memcpy(wrapped.data() + 1, pk32.data(), ed25519::PUBKEY_SIZE);

    const CScript scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(Hash160(wrapped)));
    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);
    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptPubKey, txSpend, 0, txCredit.vout[0].nValue, SigVersion::WITNESS_V0, TestSighashChainId(), &txdata);
    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));

    MutableTransactionSignatureChecker checker(&txSpend, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId());
    BOOST_CHECK(checker.CheckEd25519Signature(witSig, pk32, scriptPubKey, SigVersion::WITNESS_V0));
}

BOOST_AUTO_TEST_CASE(oru_convergence_two_convergence_outputs_rejected)
{
    const CScript oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x66);

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

BOOST_AUTO_TEST_CASE(p2sh_and_p2pkh_rejected_as_rigor_script_type)
{
    BaseSignatureChecker checker;
    ScriptError err{SCRIPT_ERR_OK};
    CScript p2sh;
    p2sh << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11) << RIGOR_RESERVED_LEGACY_EQUAL;
    BOOST_CHECK(!VerifyRigorPredicate(CScript() << RIGOR_PUSH_1, p2sh, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, checker, &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);

    CScript p2pkh;
    p2pkh << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    err = SCRIPT_ERR_OK;
    BOOST_CHECK(!VerifyRigorPredicate(CScript(), p2pkh, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, checker, &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
}

BOOST_AUTO_TEST_CASE(pk_pkh_sh_combo_descriptors_rejected)
{
    FlatSigningProvider keys;
    std::string error;
    BOOST_CHECK(!Parse("pk(0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798)", keys, error));
    BOOST_CHECK(error.find("pk()") != std::string::npos);
    BOOST_CHECK(!Parse("pkh(0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798)", keys, error));
    BOOST_CHECK(error.find("pkh()") != std::string::npos);
    BOOST_CHECK(!Parse("sh(wpkh(0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798))", keys, error));
    BOOST_CHECK(error.find("sh()") != std::string::npos);
    BOOST_CHECK(!Parse("combo(0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798)", keys, error));
    BOOST_CHECK(error.find("combo() is not supported") != std::string::npos);
    BOOST_CHECK(!Parse("sh(combo(0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798))", keys, error));
    BOOST_CHECK(error.find("sh()") != std::string::npos);
}

static void AssertCheckTransactionRejects(const CScript& spk, const char* reason)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = COIN;
    mtx.vout[0].scriptPubKey = spk;
    TxValidationState state;
    BOOST_CHECK(!CheckTransaction(CTransaction(mtx), state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), reason);
}

BOOST_AUTO_TEST_CASE(classifier_predicates_are_one_function)
{
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    const CScript res = TestNativeResScript();
    const CScript p2wsh = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x55);
    const CScript opret = CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>{0xaa};
    std::vector<unsigned char> cvg2(32, 0x00);
    std::memcpy(cvg2.data(), RigorTags::CVG2.data(), 4);
    const CScript cvg2_v1 = CScript() << RIGOR_PUSH_1 << cvg2;

    auto check_triple = [](const CScript& spk, bool allowed, AssetType asset) {
        std::vector<std::vector<unsigned char>> sols;
        const TxoutType t = ClassifyRigorTxout(spk, sols);
        BOOST_CHECK_EQUAL(Consensus::IsAllowedScript(spk), allowed);
        BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(spk), asset);
        if (allowed) {
            BOOST_CHECK(t == TxoutType::WITNESS_V0_KEYHASH || t == TxoutType::WITNESS_V1_RES ||
                        t == TxoutType::RES_MINT || t == TxoutType::CONVERGE);
        } else if (asset == ASSET_NONE) {
            BOOST_CHECK_EQUAL(t, TxoutType::NULL_DATA);
        } else {
            BOOST_CHECK_EQUAL(t, TxoutType::NONSTANDARD);
        }
    };
    check_triple(oru, true, ASSET_ORU);
    check_triple(res, true, ASSET_RES);
    check_triple(p2wsh, false, ASSET_INVALID);
    check_triple(opret, false, ASSET_NONE);
    check_triple(cvg2_v1, false, ASSET_INVALID);

    AssertCheckTransactionRejects(p2wsh, "bad-rigor-script-type");
    AssertCheckTransactionRejects(CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG, "bad-rigor-script-type");
    AssertCheckTransactionRejects(CScript() << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11) << RIGOR_RESERVED_LEGACY_EQUAL, "bad-rigor-script-type");
    CScript unknown_ver;
    unknown_ver << RIGOR_PUSH_3 << std::vector<unsigned char>(32, 0x11);
    AssertCheckTransactionRejects(unknown_ver, "bad-rigor-script-type");
    for (const size_t n : {size_t{0}, size_t{19}, size_t{21}, size_t{31}, size_t{33}}) {
        AssertCheckTransactionRejects(CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(n, 0x11), "bad-rigor-script-type");
    }
    AssertCheckTransactionRejects(CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(20, 0x11), "bad-rigor-script-type");
    AssertCheckTransactionRejects(CScript() << std::vector<unsigned char>(32, 0x11) << RIGOR_RESERVED_LEGACY_CHECKSIG, "bad-rigor-script-type");

    CMutableTransaction mix_mtx;
    mix_mtx.vin.resize(1);
    mix_mtx.vout.emplace_back(COIN, oru);
    mix_mtx.vout.emplace_back(2 * COIN, res);
    mix_mtx.vout.emplace_back(0, opret);
    const CTransaction mix(mix_mtx);
    BOOST_CHECK_EQUAL(mix.GetValueOut(ASSET_ORU) + mix.GetValueOut(ASSET_RES) + mix.GetValueOut(ASSET_NONE) + mix.GetValueOut(ASSET_INVALID), mix.GetValueOut());
}

BOOST_AUTO_TEST_SUITE_END()
