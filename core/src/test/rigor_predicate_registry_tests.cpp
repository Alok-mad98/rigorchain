// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <hash.h>
#include <key.h>
#include <pubkey.h>
#include <script/predicate_verify.h>
#include <script/rigor_predicate_registry.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <test/util/script.h>
#include <test/util/setup_common.h>
#include <test/util/transaction_utils.h>
#include <uint256.h>

#include <cstring>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(rigor_predicate_registry_tests, BasicTestingSetup)

static CScript ScriptP2PK(const CPubKey& pk)
{
    return CScript() << ToByteVector(pk) << RIGOR_RESERVED_LEGACY_CHECKSIG;
}

static CScript ScriptP2PKH(const CPubKey& pk)
{
    return CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160
                     << ToByteVector(pk.GetID()) << RIGOR_RESERVED_LEGACY_EQUALVERIFY
                     << RIGOR_RESERVED_LEGACY_CHECKSIG;
}

static CScript ScriptMultisig(const CPubKey& pk)
{
    return CScript() << RIGOR_PUSH_1 << ToByteVector(pk) << RIGOR_PUSH_1 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
}

static CScript ScriptP2SH()
{
    return CScript() << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11)
                     << RIGOR_RESERVED_LEGACY_EQUAL;
}

static CScript ScriptP2WSH()
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x22);
}

static CScript ScriptTaprootShaped()
{
    std::vector<unsigned char> prog(32, 0x33);
    std::memcpy(prog.data(), RigorTags::CVG2.data(), 4);
    return CScript() << RIGOR_PUSH_1 << prog;
}

static CScript ScriptWitnessV2()
{
    return CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(32, 0x44);
}

BOOST_AUTO_TEST_CASE(live_registry_count_is_five)
{
    BOOST_CHECK_EQUAL(LIVE_RIGOR_PREDICATE_COUNT, 5U);
}

BOOST_AUTO_TEST_CASE(bitcoin_templates_are_not_predicates)
{
    CKey key;
    key.MakeNewKey(true);
    const CPubKey pk = key.GetPubKey();

    BOOST_CHECK(!LookupRigorPredicate(ScriptP2PK(pk)));
    BOOST_CHECK(!LookupRigorPredicate(ScriptP2PKH(pk)));
    BOOST_CHECK(!LookupRigorPredicate(ScriptMultisig(pk)));
    BOOST_CHECK(!LookupRigorPredicate(ScriptP2SH()));
    BOOST_CHECK(!LookupRigorPredicate(ScriptP2WSH()));
    BOOST_CHECK(!LookupRigorPredicate(ScriptTaprootShaped()));
    BOOST_CHECK(!LookupRigorPredicate(ScriptWitnessV2()));
    BOOST_CHECK(!LookupRigorPredicate(CScript()));
}

BOOST_AUTO_TEST_CASE(bitcoin_template_spends_fail_closed)
{
    CKey key;
    key.MakeNewKey(true);
    const CPubKey pk = key.GetPubKey();
    const uint32_t flags = SCRIPT_VERIFY_WITNESS;
    ScriptError err;
    const CScript empty;

    auto reject = [&](const CScript& spk, ScriptError expected) {
        err = SCRIPT_ERR_OK;
        BOOST_CHECK(!VerifyRigorRegistryPredicate(empty, spk, nullptr, flags, BaseSignatureChecker(), &err));
        BOOST_CHECK_EQUAL(err, expected);
    };
    reject(ScriptP2PK(pk), SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    reject(ScriptP2PKH(pk), SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    reject(ScriptMultisig(pk), SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    reject(ScriptP2SH(), SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    reject(ScriptP2WSH(), SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH);
    reject(ScriptTaprootShaped(), SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM);
    reject(ScriptWitnessV2(), SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
}

BOOST_AUTO_TEST_CASE(live_templates_map_to_registry_ids)
{
    CKey key;
    key.MakeNewKey(true);
    const CScript oru = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey()));
    BOOST_CHECK(LookupRigorPredicate(oru) == RigorPredicateID::ORU_V0_KEYHASH);
    BOOST_CHECK(IsLiveRigorPredicateSpendable(*LookupRigorPredicate(oru)));

    const CScript res = TestNativeResScript();
    BOOST_CHECK(LookupRigorPredicate(res) == RigorPredicateID::RES_V1_NATIVE_KEY);

    const CScript mint = GetResMintScript(/*maturity_height=*/0, Hash160(TestEd25519PubKey()));
    BOOST_CHECK(LookupRigorPredicate(mint) == RigorPredicateID::RES_MINT_V0);
    BOOST_CHECK(IsLiveRigorPredicateSpendable(*LookupRigorPredicate(mint)));

    const CScript conv = GetConvergeScript(Hash160(TestEd25519PubKey()));
    BOOST_CHECK(LookupRigorPredicate(conv) == RigorPredicateID::CONVERGE_V0);
    BOOST_CHECK(!IsLiveRigorPredicateSpendable(*LookupRigorPredicate(conv)));

    const CScript burn = CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>{'x'};
    BOOST_CHECK(LookupRigorPredicate(burn) == RigorPredicateID::OP_RETURN_BURN);
    BOOST_CHECK(!IsLiveRigorPredicateSpendable(*LookupRigorPredicate(burn)));
}

BOOST_AUTO_TEST_CASE(oru_spend_succeeds)
{
    FillableSigningProvider keystore;
    CKey key;
    key.MakeNewKey(true);
    BOOST_CHECK(keystore.AddKey(key));
    const CScript spk = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey()));
    BOOST_REQUIRE(LookupRigorPredicate(spk) == RigorPredicateID::ORU_V0_KEYHASH);

    CMutableTransaction txFrom = BuildCreditingTransaction(spk, COIN);
    CMutableTransaction txTo = BuildSpendingTransaction(CScript(), CScriptWitness(), CTransaction(txFrom));
    SignatureData dummy;
    BOOST_CHECK(SignSignature(keystore, CTransaction(txFrom), txTo, 0, uint256::ZERO, dummy));
    ScriptError err;
    PrecomputedTransactionData txdata;
    txdata.Init(txTo, {txFrom.vout[0]});
    BOOST_CHECK(VerifyRigorRegistryPredicate(txTo.vin[0].scriptSig, spk, &txTo.vin[0].scriptWitness,
        SCRIPT_VERIFY_WITNESS,
        MutableTransactionSignatureChecker(&txTo, 0, txFrom.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, uint256::ZERO), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(res_spend_succeeds)
{
    CKey key;
    key.MakeNewKey(true);
    const std::vector<unsigned char> pk = key.GetPubKey().GetEd25519Payload();
    const CScript spk = CScript() << RIGOR_PUSH_1 << pk;
    BOOST_REQUIRE(LookupRigorPredicate(spk) == RigorPredicateID::RES_V1_NATIVE_KEY);

    std::vector<unsigned char> sig(64, 0x11);
    CScriptWitness wit;
    wit.stack.push_back(sig);
    wit.stack.push_back(pk);
    class OkSig : public BaseSignatureChecker {
    public:
        bool CheckEd25519Signature(const std::vector<unsigned char>&, const std::vector<unsigned char>&,
                                   const CScript&, SigVersion) const override { return true; }
    };
    ScriptError err{SCRIPT_ERR_OK};
    BOOST_CHECK(VerifyRigorRegistryPredicate(CScript(), spk, &wit, SCRIPT_VERIFY_WITNESS, OkSig(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(res_spend_rejects_bad_sig_mismatch_and_encoding)
{
    CKey key;
    key.MakeNewKey(true);
    const std::vector<unsigned char> pk = key.GetPubKey().GetEd25519Payload();
    const CScript spk = CScript() << RIGOR_PUSH_1 << pk;
    class BadSig : public BaseSignatureChecker {
    public:
        bool CheckEd25519Signature(const std::vector<unsigned char>&, const std::vector<unsigned char>&,
                                   const CScript&, SigVersion) const override { return false; }
    };

    {
        CScriptWitness wit;
        wit.stack.emplace_back(64, 0x11);
        wit.stack.push_back(pk);
        ScriptError err{SCRIPT_ERR_OK};
        BOOST_CHECK(!VerifyRigorRegistryPredicate(CScript(), spk, &wit, SCRIPT_VERIFY_WITNESS, BadSig(), &err));
        BOOST_CHECK_EQUAL(err, SCRIPT_ERR_EVAL_FALSE);
    }
    {
        CScriptWitness wit;
        wit.stack.emplace_back(64, 0x11);
        wit.stack.emplace_back(32, 0x00);
        ScriptError err{SCRIPT_ERR_OK};
        BOOST_CHECK(!VerifyRigorRegistryPredicate(CScript(), spk, &wit, SCRIPT_VERIFY_WITNESS, BadSig(), &err));
        BOOST_CHECK_EQUAL(err, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
    }
    {
        CScriptWitness wit;
        wit.stack.emplace_back(32, 0x11);
        wit.stack.push_back(pk);
        ScriptError err{SCRIPT_ERR_OK};
        BOOST_CHECK(!VerifyRigorRegistryPredicate(CScript(), spk, &wit, SCRIPT_VERIFY_WITNESS, BadSig(), &err));
        BOOST_CHECK_EQUAL(err, SCRIPT_ERR_SIG_DER);
    }
}

BOOST_AUTO_TEST_CASE(res_mint_spend_succeeds_when_mature)
{
    const std::vector<unsigned char> pk = TestEd25519PubKey();
    const CScript mint = GetResMintScript(/*maturity_height=*/10, Hash160(pk));
    BOOST_REQUIRE(LookupRigorPredicate(mint) == RigorPredicateID::RES_MINT_V0);

    std::vector<unsigned char> sig(64, 0x22);
    const CScript scriptSig = CScript() << sig << pk;
    class MatureOk : public BaseSignatureChecker {
    public:
        int GetSpendHeight() const override { return 10; }
        bool CheckEd25519Signature(const std::vector<unsigned char>&, const std::vector<unsigned char>&,
                                   const CScript&, SigVersion) const override { return true; }
    };
    ScriptError err{SCRIPT_ERR_OK};
    BOOST_CHECK(VerifyRigorRegistryPredicate(scriptSig, mint, nullptr, SCRIPT_VERIFY_NONE, MatureOk(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(res_mint_spend_rejects_immature_and_bad_sig)
{
    const std::vector<unsigned char> pk = TestEd25519PubKey();
    const CScript mint = GetResMintScript(/*maturity_height=*/10, Hash160(pk));
    const CScript scriptSig = CScript() << std::vector<unsigned char>(64, 0x22) << pk;

    class ImmatureOk : public BaseSignatureChecker {
    public:
        int GetSpendHeight() const override { return 9; }
        bool CheckEd25519Signature(const std::vector<unsigned char>&, const std::vector<unsigned char>&,
                                   const CScript&, SigVersion) const override { return true; }
    };
    class MatureBad : public BaseSignatureChecker {
    public:
        int GetSpendHeight() const override { return 10; }
        bool CheckEd25519Signature(const std::vector<unsigned char>&, const std::vector<unsigned char>&,
                                   const CScript&, SigVersion) const override { return false; }
    };

    ScriptError err{SCRIPT_ERR_OK};
    BOOST_CHECK(!VerifyRigorRegistryPredicate(scriptSig, mint, nullptr, SCRIPT_VERIFY_NONE, ImmatureOk(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_UNSATISFIED_LOCKTIME);

    err = SCRIPT_ERR_OK;
    BOOST_CHECK(!VerifyRigorRegistryPredicate(scriptSig, mint, nullptr, SCRIPT_VERIFY_NONE, MatureBad(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_EVAL_FALSE);
}

BOOST_AUTO_TEST_CASE(converge_and_return_are_burns_not_spends)
{
    const CScript conv = GetConvergeScript(Hash160(TestEd25519PubKey()));
    const CScript opret = CScript() << RIGOR_MARKER_RETURN;
    ScriptError err{SCRIPT_ERR_OK};
    BOOST_CHECK(!VerifyRigorRegistryPredicate(CScript(), conv, nullptr, SCRIPT_VERIFY_WITNESS, BaseSignatureChecker(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    err = SCRIPT_ERR_OK;
    BOOST_CHECK(!VerifyRigorRegistryPredicate(CScript(), opret, nullptr, SCRIPT_VERIFY_WITNESS, BaseSignatureChecker(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
    BOOST_CHECK(LookupRigorPredicate(conv) == RigorPredicateID::CONVERGE_V0);
    BOOST_CHECK(LookupRigorPredicate(opret) == RigorPredicateID::OP_RETURN_BURN);
}

BOOST_AUTO_TEST_SUITE_END()
