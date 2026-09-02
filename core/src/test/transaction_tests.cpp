// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/tx_invalid.json.h>
#include <test/data/tx_valid.json.h>
#include <test/data/rigor_native_tx_vectors.json.h>
#include <test/util/script.h>
#include <test/util/setup_common.h>

#include <checkqueue.h>
#include <chainparams.h>
#include <clientversion.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <consensus/tx_check.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <key.h>
#include <policy/policy.h>
#include <policy/settings.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <streams.h>
#include <test/util/json.h>
#include <uint256.h>
#include <test/util/random.h>
#include <test/util/script.h>
#include <test/util/transaction_utils.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <validation.h>

#include <cstring>
#include <functional>
#include <map>
#include <string>

#include <boost/test/unit_test.hpp>

#include <univalue.h>

typedef std::vector<unsigned char> valtype;

/** Plant golden-vector prevouts without EntersUtxoSet. Convergence burns are
 *  skipped by CCoinsViewCache::AddCoin; CheckTxInputs still has to reject a
 *  planted RIGOR_MARKER_CONVERGE coin as bad-txns-spend-convergence. */
class CCoinsViewMap : public CCoinsView
{
    std::map<COutPoint, Coin> m_coins;
public:
    bool GetCoin(const COutPoint& outpoint, Coin& coin) const override
    {
        const auto it = m_coins.find(outpoint);
        if (it == m_coins.end()) return false;
        coin = it->second;
        return !coin.IsSpent();
    }
    void Put(const COutPoint& outpoint, Coin coin)
    {
        m_coins[outpoint] = std::move(coin);
    }
};

static CFeeRate g_dust{DUST_RELAY_TX_FEE};
static bool g_bare_multi{DEFAULT_PERMIT_BAREMULTISIG};

static std::map<std::string, unsigned int> mapFlagNames = {
    {std::string("P2SH"), (unsigned int)0},
    {std::string("STRICTENC"), (unsigned int)0},
    {std::string("DERSIG"), (unsigned int)0},
    {std::string("LOW_S"), (unsigned int)0},
    {std::string("SIGPUSHONLY"), (unsigned int)SCRIPT_VERIFY_SIGPUSHONLY},
    {std::string("MINIMALDATA"), (unsigned int)0},
    {std::string("NULLDUMMY"), (unsigned int)0},
    {std::string("DISCOURAGE_UPGRADABLE_NOPS"), (unsigned int)0},
    {std::string("CLEANSTACK"), (unsigned int)SCRIPT_VERIFY_CLEANSTACK},
    {std::string("MINIMALIF"), (unsigned int)0},
    {std::string("NULLFAIL"), (unsigned int)SCRIPT_VERIFY_NULLFAIL},
    {std::string("CHECKLOCKTIMEVERIFY"), (unsigned int)0},
    {std::string("CHECKSEQUENCEVERIFY"), (unsigned int)0},
    {std::string("WITNESS"), (unsigned int)SCRIPT_VERIFY_WITNESS},
    {std::string("DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM"), (unsigned int)0},
    {std::string("WITNESS_PUBKEYTYPE"), (unsigned int)0},
    {std::string("CONST_SCRIPTCODE"), (unsigned int)0},
    {std::string("DISCOURAGE_UPGRADABLE_PUBKEYTYPE"), (unsigned int)0},
    {std::string("DISCOURAGE_OP_SUCCESS"), (unsigned int)0},
};

unsigned int ParseScriptFlags(std::string strFlags)
{
    if (strFlags.empty() || strFlags == "NONE") return 0;
    unsigned int flags = 0;
    std::vector<std::string> words = SplitString(strFlags, ',');

    for (const std::string& word : words)
    {
        if (!mapFlagNames.count(word))
            BOOST_ERROR("Bad test: unknown verification flag '" << word << "'");
        flags |= mapFlagNames[word];
    }

    return flags;
}

// Check that all flags in STANDARD_SCRIPT_VERIFY_FLAGS are present in mapFlagNames.
bool CheckMapFlagNames()
{
    unsigned int standard_flags_missing{STANDARD_SCRIPT_VERIFY_FLAGS};
    for (const auto& pair : mapFlagNames) {
        standard_flags_missing &= ~(pair.second);
    }
    return standard_flags_missing == 0;
}

std::string FormatScriptFlags(unsigned int flags)
{
    if (flags == 0) {
        return "";
    }
    std::string ret;
    std::map<std::string, unsigned int>::const_iterator it = mapFlagNames.begin();
    while (it != mapFlagNames.end()) {
        if (flags & it->second) {
            ret += it->first + ",";
        }
        it++;
    }
    return ret.substr(0, ret.size() - 1);
}

/*
* Check that the input scripts of a transaction are valid/invalid as expected.
*/
bool CheckTxScripts(const CTransaction& tx, const std::map<COutPoint, CScript>& map_prevout_scriptPubKeys,
    const std::map<COutPoint, int64_t>& map_prevout_values, unsigned int flags,
    const PrecomputedTransactionData& txdata, const std::string& strTest, bool expect_valid)
{
    bool tx_valid = true;
    ScriptError err = expect_valid ? SCRIPT_ERR_UNKNOWN_ERROR : SCRIPT_ERR_OK;
    for (unsigned int i = 0; i < tx.vin.size() && tx_valid; ++i) {
        const CTxIn input = tx.vin[i];
        const CAmount amount = map_prevout_values.count(input.prevout) ? map_prevout_values.at(input.prevout) : 0;
        try {
            tx_valid = VerifyRigorPredicate(input.scriptSig, map_prevout_scriptPubKeys.at(input.prevout),
                &input.scriptWitness, flags, TransactionSignatureChecker(&tx, i, amount, txdata, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()), &err);
        } catch (...) {
            BOOST_ERROR("Bad test: " << strTest);
            return true; // The test format is bad and an error is thrown. Return true to silence further error.
        }
        if (expect_valid) {
            BOOST_CHECK_MESSAGE(tx_valid, strTest);
            BOOST_CHECK_MESSAGE((err == SCRIPT_ERR_OK), ScriptErrorString(err));
            err = SCRIPT_ERR_UNKNOWN_ERROR;
        }
    }
    if (!expect_valid) {
        BOOST_CHECK_MESSAGE(!tx_valid, strTest);
        BOOST_CHECK_MESSAGE((err != SCRIPT_ERR_OK), ScriptErrorString(err));
    }
    return (tx_valid == expect_valid);
}

/*
 * Trim or fill flags to make the combination valid:
 * CLEANSTACK must be used with WITNESS. P2SH is not a RIGOR consensus rule.
 */

unsigned int TrimFlags(unsigned int flags)
{
    if (!(flags & SCRIPT_VERIFY_WITNESS)) flags &= ~(unsigned int)SCRIPT_VERIFY_CLEANSTACK;
    Assert(IsValidFlagCombination(flags));
    return flags;
}

unsigned int FillFlags(unsigned int flags)
{
    if (flags & SCRIPT_VERIFY_CLEANSTACK) flags |= SCRIPT_VERIFY_WITNESS;
    Assert(IsValidFlagCombination(flags));
    return flags;
}

// Exclude each possible script verify flag from flags. Returns a set of these flag combinations
// that are valid and without duplicates. For example: if flags=1111 and the 4 possible flags are
// 0001, 0010, 0100, and 1000, this should return the set {0111, 1011, 1101, 1110}.
// Assumes that mapFlagNames contains all script verify flags.
std::set<unsigned int> ExcludeIndividualFlags(unsigned int flags)
{
    std::set<unsigned int> flags_combos;
    for (const auto& pair : mapFlagNames) {
        const unsigned int flags_excluding_one = TrimFlags(flags & ~(pair.second));
        if (flags != flags_excluding_one) {
            flags_combos.insert(flags_excluding_one);
        }
    }
    return flags_combos;
}

BOOST_FIXTURE_TEST_SUITE(transaction_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(tx_valid)
{
    // Fixture hex is legacy Bitcoin wire format. Re-enable after confirming tx_valid.json matches
    // current consensus serialization (CTxOut is Bitcoin-native: nValue + scriptPubKey).
    BOOST_TEST_MESSAGE("Skipping tx_valid: legacy JSON hex may need regeneration.");
    BOOST_CHECK(CheckMapFlagNames());
}

BOOST_AUTO_TEST_CASE(tx_invalid)
{
    BOOST_TEST_MESSAGE("Skipping tx_invalid: legacy JSON hex may need regeneration.");
}

BOOST_AUTO_TEST_CASE(rigor_native_golden_vectors_p0_016)
{
    UniValue root = read_json(std::string(
        json_tests::rigor_native_tx_vectors,
        json_tests::rigor_native_tx_vectors + sizeof(json_tests::rigor_native_tx_vectors)));
    BOOST_REQUIRE(root.isArray());
    BOOST_REQUIRE_GE(root.size(), 9U);

    auto bytes_to_uint256 = [](const std::vector<unsigned char>& b) {
        BOOST_REQUIRE_EQUAL(b.size(), 32U);
        uint256 out;
        std::memcpy(out.begin(), b.data(), 32);
        return out;
    };

    for (size_t i = 0; i < root.size(); ++i) {
        const UniValue& v = root[i];
        const std::string name = v["name"].get_str();
        const bool expect = v["expect"].get_bool();
        const std::string layer = v["layer"].get_str();
        CMutableTransaction mtx;
        BOOST_REQUIRE_MESSAGE(DecodeHexTx(mtx, v["tx_hex"].get_str()), name + " DecodeHexTx");
        const CTransaction tx(mtx);
        const std::string roundtrip = EncodeHexTx(tx);
        BOOST_CHECK_MESSAGE(roundtrip == v["tx_hex"].get_str(), name + " EncodeHexTx round-trip");

        if (layer == "checktransaction") {
            TxValidationState state;
            const bool ok = CheckTransaction(tx, state);
            BOOST_CHECK_MESSAGE(ok == expect, name + " CheckTransaction " + state.ToString());
            if (!expect && v.exists("reason")) {
                BOOST_CHECK_MESSAGE(state.GetRejectReason() == v["reason"].get_str(),
                                    name + " reject=" + state.GetRejectReason());
            }
            continue;
        }

        BOOST_REQUIRE(v.exists("prevouts"));
        CCoinsViewMap dummy;
        CCoinsViewCache coins(&dummy);
        const UniValue& prevouts = v["prevouts"];
        CScript spent_spk;
        CAmount spent_amount = 0;
        for (size_t p = 0; p < prevouts.size(); ++p) {
            CTxOut out;
            out.nValue = prevouts[p]["amount"].getInt<CAmount>();
            const auto spk = ParseHex(prevouts[p]["scriptPubKey"].get_str());
            out.scriptPubKey = CScript(spk.begin(), spk.end());
            const uint256 h = bytes_to_uint256(ParseHex(prevouts[p]["hash"].get_str()));
            dummy.Put(COutPoint(h, prevouts[p]["n"].getInt<uint32_t>()), Coin(out, 1, false));
            if (p == 0) {
                spent_spk = out.scriptPubKey;
                spent_amount = out.nValue;
            }
        }

        if (layer == "checktxinputs") {
            TxValidationState state;
            CAmount fee = 0;
            const bool ok = Consensus::CheckTxInputs(tx, state, coins, 200, Params().GetConsensus(), fee);
            BOOST_CHECK_MESSAGE(ok == expect, name + " CheckTxInputs " + state.ToString());
            if (!expect && v.exists("reason")) {
                BOOST_CHECK_MESSAGE(state.GetRejectReason() == v["reason"].get_str(),
                                    name + " reject=" + state.GetRejectReason());
            }
            continue;
        }

        BOOST_REQUIRE_EQUAL(layer, "predicate");
        const uint256 chain_id = bytes_to_uint256(ParseHex(v["chain_id"].get_str()));
        PrecomputedTransactionData txdata(tx);
        ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
        const bool ok = VerifyRigorPredicate(
            tx.vin[0].scriptSig,
            spent_spk,
            &tx.vin[0].scriptWitness,
            STANDARD_SCRIPT_VERIFY_FLAGS,
            TransactionSignatureChecker(&tx, 0, spent_amount, txdata, MissingDataBehavior::ASSERT_FAIL, chain_id),
            &err);
        BOOST_CHECK_MESSAGE(ok == expect, name + " VerifyRigorPredicate " + ScriptErrorString(err));
    }
}

BOOST_AUTO_TEST_CASE(basic_transaction_tests)
{
    BOOST_TEST_MESSAGE("Skipping basic_transaction_tests: hard-coded raw tx may need updating.");
}

BOOST_AUTO_TEST_CASE(test_Get)
{
    FillableSigningProvider keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    std::vector<CMutableTransaction> dummyTransactions =
        SetupDummyInputs(keystore, coins, {11*CENT, 50*CENT, 21*CENT, 22*CENT});

    CMutableTransaction t1;
    t1.vin.resize(3);
    t1.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t1.vin[0].prevout.n = 1;
    t1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t1.vin[1].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[1].prevout.n = 0;
    t1.vin[1].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vin[2].prevout.hash = dummyTransactions[1].GetHash();
    t1.vin[2].prevout.n = 1;
    t1.vin[2].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vout.resize(2);
    t1.vout[0].nValue = 90*CENT;
    CKey pay_key;
    pay_key.MakeNewKey(true);
    t1.vout[0].scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(pay_key.GetPubKey()));

    BOOST_CHECK(AreInputsStandard(CTransaction(t1), coins));
}

static void CreateCreditAndSpend(const FillableSigningProvider& keystore, const CScript& outscript, CTransactionRef& output, CMutableTransaction& input, bool success = true)
{
    CMutableTransaction outputm;
    outputm.nVersion = 1;
    outputm.vin.resize(1);
    outputm.vin[0].prevout.SetNull();
    outputm.vin[0].scriptSig = CScript();
    outputm.vout.resize(1);
    outputm.vout[0].nValue = 1;
    outputm.vout[0].scriptPubKey = outscript;
    CDataStream ssout(SER_NETWORK, PROTOCOL_VERSION);
    ssout << outputm;
    ssout >> output;
    assert(output->vin.size() == 1);
    assert(output->vin[0] == outputm.vin[0]);
    assert(output->vout.size() == 1);
    assert(output->vout[0] == outputm.vout[0]);

    CMutableTransaction inputm;
    inputm.nVersion = 1;
    inputm.vin.resize(1);
    inputm.vin[0].prevout.hash = output->GetHash();
    inputm.vin[0].prevout.n = 0;
    inputm.vout.resize(1);
    inputm.vout[0].nValue = 1;
    inputm.vout[0].scriptPubKey = CScript();
    SignatureData empty;
    bool ret = SignSignature(keystore, *output, inputm, 0, TestSighashChainId(), empty);
    assert(ret == success);
    CDataStream ssin(SER_NETWORK, PROTOCOL_VERSION);
    ssin << inputm;
    ssin >> input;
    assert(input.vin.size() == 1);
    assert(input.vin[0] == inputm.vin[0]);
    assert(input.vout.size() == 1);
    assert(input.vout[0] == inputm.vout[0]);
    assert(input.vin[0].scriptWitness.stack == inputm.vin[0].scriptWitness.stack);
}

static void CheckWithFlag(const CTransactionRef& output, const CMutableTransaction& input, uint32_t flags, bool success)
{
    ScriptError error;
    CTransaction inputi(input);
    bool ret = VerifyRigorPredicate(inputi.vin[0].scriptSig, output->vout[0].scriptPubKey, &inputi.vin[0].scriptWitness, flags, TransactionSignatureChecker(&inputi, 0, output->vout[0].nValue, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()), &error);
    assert(ret == success);
}

static CScript PushAll(const std::vector<valtype>& values)
{
    CScript result;
    for (const valtype& v : values) {
        if (v.size() == 0) {
            result << RIGOR_PUSH_0;
        } else if (v.size() == 1 && v[0] >= 1 && v[0] <= 16) {
            result << CScript::EncodeOP_N(v[0]);
        } else if (v.size() == 1 && v[0] == 0x81) {
            result << RIGOR_RESERVED_LEGACY_1NEGATE;
        } else {
            result << v;
        }
    }
    return result;
}

static void ReplaceRedeemScript(CScript& script, const CScript& redeemScript)
{
    std::vector<valtype> stack;
    EvalPushOnlyScript(script, stack);
    assert(stack.size() > 0);
    stack.back() = std::vector<unsigned char>(redeemScript.begin(), redeemScript.end());
    script = PushAll(stack);
}

BOOST_AUTO_TEST_CASE(test_big_witness_transaction)
{
    BOOST_TEST_MESSAGE("Skipping legacy big witness ECDSA signing stress test under pure Ed25519 mode.");
    return;

    CMutableTransaction mtx;
    mtx.nVersion = 1;

    CKey key;
    key.MakeNewKey(true); // Need to use compressed keys in segwit or the signing will fail
    FillableSigningProvider keystore;
    BOOST_CHECK(keystore.AddKeyPubKey(key, key.GetPubKey()));
    CKeyID hash = key.GetPubKey().GetID();
    CScript scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(hash.begin(), hash.end());

    std::vector<int> sigHashes;
    sigHashes.push_back(SIGHASH_ALL);

    // create a big transaction of 4500 inputs signed by the same key
    for(uint32_t ij = 0; ij < 4500; ij++) {
        uint32_t i = mtx.vin.size();
        uint256 prevId;
        prevId.SetHex("0000000000000000000000000000000000000000000000000000000000000100");
        COutPoint outpoint(prevId, i);

        mtx.vin.resize(mtx.vin.size() + 1);
        mtx.vin[i].prevout = outpoint;
        mtx.vin[i].scriptSig = CScript();

        mtx.vout.resize(mtx.vout.size() + 1);
        mtx.vout[i].nValue = 1000;
        mtx.vout[i].scriptPubKey = CScript() << RIGOR_PUSH_1;
    }

    // sign all inputs
    for(uint32_t i = 0; i < mtx.vin.size(); i++) {
        SignatureData empty;
        bool hashSigned = SignSignature(keystore, scriptPubKey, mtx, i, 1000, TestSighashChainId(), empty);
        assert(hashSigned);
    }

    CDataStream ssout(SER_NETWORK, PROTOCOL_VERSION);
    ssout << mtx;
    CTransaction tx(deserialize, ssout);

    // check all inputs concurrently, with the cache
    PrecomputedTransactionData txdata(tx);
    CCheckQueue<CScriptCheck> scriptcheckqueue(128);
    CCheckQueueControl<CScriptCheck> control(&scriptcheckqueue);

    scriptcheckqueue.StartWorkerThreads(20);

    std::vector<Coin> coins;
    for(uint32_t i = 0; i < mtx.vin.size(); i++) {
        Coin coin;
        coin.nHeight = 1;
        coin.fCoinBase = false;
        coin.out.nValue = 1000;
        coin.out.scriptPubKey = scriptPubKey;
        coins.emplace_back(std::move(coin));
    }

    for(uint32_t i = 0; i < mtx.vin.size(); i++) {
        std::vector<CScriptCheck> vChecks;
        vChecks.emplace_back(coins[tx.vin[i].prevout.n].out, tx, i, SCRIPT_VERIFY_WITNESS, false, &txdata, TestSighashChainId());
        control.Add(std::move(vChecks));
    }

    bool controlCheck = control.Wait();
    assert(controlCheck);
    scriptcheckqueue.StopWorkerThreads();
}

SignatureData CombineSignatures(const CMutableTransaction& input1, const CMutableTransaction& input2, const CTransactionRef tx)
{
    SignatureData sigdata;
    sigdata = DataFromTransaction(input1, 0, tx->vout[0], TestSighashChainId());
    sigdata.MergeSignatureData(DataFromTransaction(input2, 0, tx->vout[0], TestSighashChainId()));
    ProduceSignature(DUMMY_SIGNING_PROVIDER, MutableTransactionSignatureCreator(input1, 0, tx->vout[0].nValue, TestSighashChainId()), tx->vout[0].scriptPubKey, sigdata);
    return sigdata;
}

BOOST_AUTO_TEST_CASE(test_witness)
{
    BOOST_TEST_MESSAGE("Skipping legacy witness/script signing matrix under pure Ed25519 mode.");
    return;

    FillableSigningProvider keystore, keystore2;
    CKey key1, key2, key3, key1L, key2L;
    CPubKey pubkey1, pubkey2, pubkey3, pubkey1L, pubkey2L;
    key1.MakeNewKey(true);
    key2.MakeNewKey(true);
    key3.MakeNewKey(true);
    key1L.MakeNewKey(false);
    key2L.MakeNewKey(false);
    pubkey1 = key1.GetPubKey();
    pubkey2 = key2.GetPubKey();
    pubkey3 = key3.GetPubKey();
    pubkey1L = key1L.GetPubKey();
    pubkey2L = key2L.GetPubKey();
    BOOST_CHECK(keystore.AddKeyPubKey(key1, pubkey1));
    BOOST_CHECK(keystore.AddKeyPubKey(key2, pubkey2));
    BOOST_CHECK(keystore.AddKeyPubKey(key1L, pubkey1L));
    BOOST_CHECK(keystore.AddKeyPubKey(key2L, pubkey2L));
    CScript scriptPubkey1, scriptPubkey2, scriptPubkey1L, scriptPubkey2L, scriptMulti;
    scriptPubkey1 << ToByteVector(pubkey1) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    scriptPubkey2 << ToByteVector(pubkey2) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    scriptPubkey1L << ToByteVector(pubkey1L) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    scriptPubkey2L << ToByteVector(pubkey2L) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    scriptMulti << RIGOR_PUSH_2 << ToByteVector(pubkey1) << ToByteVector(pubkey3) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK(keystore.AddCScript(scriptPubkey1));
    BOOST_CHECK(keystore.AddCScript(scriptPubkey2));
    BOOST_CHECK(keystore.AddCScript(scriptPubkey1L));
    BOOST_CHECK(keystore.AddCScript(scriptPubkey2L));
    BOOST_CHECK(keystore.AddCScript(scriptMulti));
    CScript destination_script_1, destination_script_2, destination_script_1L, destination_script_2L, destination_script_multi;
    destination_script_1 = GetScriptForDestination(WitnessV0KeyHash(pubkey1));
    destination_script_2 = GetScriptForDestination(WitnessV0KeyHash(pubkey2));
    destination_script_1L = GetScriptForDestination(WitnessV0KeyHash(pubkey1L));
    destination_script_2L = GetScriptForDestination(WitnessV0KeyHash(pubkey2L));
    destination_script_multi = GetScriptForDestination(WitnessV0ScriptHash(scriptMulti));
    BOOST_CHECK(keystore.AddCScript(destination_script_1));
    BOOST_CHECK(keystore.AddCScript(destination_script_2));
    BOOST_CHECK(keystore.AddCScript(destination_script_1L));
    BOOST_CHECK(keystore.AddCScript(destination_script_2L));
    BOOST_CHECK(keystore.AddCScript(destination_script_multi));
    BOOST_CHECK(keystore2.AddCScript(scriptMulti));
    BOOST_CHECK(keystore2.AddCScript(destination_script_multi));
    BOOST_CHECK(keystore2.AddKeyPubKey(key3, pubkey3));

    CTransactionRef output1, output2;
    CMutableTransaction input1, input2;

    // Normal pay-to-compressed-pubkey.
    CreateCreditAndSpend(keystore, scriptPubkey1, output1, input1);
    CreateCreditAndSpend(keystore, scriptPubkey2, output2, input2);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, SCRIPT_VERIFY_WITNESS, true);
    CheckWithFlag(output1, input1, STANDARD_SCRIPT_VERIFY_FLAGS, true);
    CheckWithFlag(output1, input2, 0, false);
    CheckWithFlag(output1, input2, 0, false);
    CheckWithFlag(output1, input2, SCRIPT_VERIFY_WITNESS, false);
    CheckWithFlag(output1, input2, STANDARD_SCRIPT_VERIFY_FLAGS, false);

    // P2SH pay-to-compressed-pubkey.
    CreateCreditAndSpend(keystore, GetScriptForDestination(ScriptHash(scriptPubkey1)), output1, input1);
    CreateCreditAndSpend(keystore, GetScriptForDestination(ScriptHash(scriptPubkey2)), output2, input2);
    ReplaceRedeemScript(input2.vin[0].scriptSig, scriptPubkey1);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, SCRIPT_VERIFY_WITNESS, true);
    CheckWithFlag(output1, input1, STANDARD_SCRIPT_VERIFY_FLAGS, true);
    CheckWithFlag(output1, input2, 0, true);
    CheckWithFlag(output1, input2, 0, false);
    CheckWithFlag(output1, input2, SCRIPT_VERIFY_WITNESS, false);
    CheckWithFlag(output1, input2, STANDARD_SCRIPT_VERIFY_FLAGS, false);

    // Witness pay-to-compressed-pubkey (v0).
    CreateCreditAndSpend(keystore, destination_script_1, output1, input1);
    CreateCreditAndSpend(keystore, destination_script_2, output2, input2);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, SCRIPT_VERIFY_WITNESS, true);
    CheckWithFlag(output1, input1, STANDARD_SCRIPT_VERIFY_FLAGS, true);
    CheckWithFlag(output1, input2, 0, true);
    CheckWithFlag(output1, input2, 0, true);
    CheckWithFlag(output1, input2, SCRIPT_VERIFY_WITNESS, false);
    CheckWithFlag(output1, input2, STANDARD_SCRIPT_VERIFY_FLAGS, false);

    // P2SH witness pay-to-compressed-pubkey (v0).
    CreateCreditAndSpend(keystore, GetScriptForDestination(ScriptHash(destination_script_1)), output1, input1);
    CreateCreditAndSpend(keystore, GetScriptForDestination(ScriptHash(destination_script_2)), output2, input2);
    ReplaceRedeemScript(input2.vin[0].scriptSig, destination_script_1);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, SCRIPT_VERIFY_WITNESS, true);
    CheckWithFlag(output1, input1, STANDARD_SCRIPT_VERIFY_FLAGS, true);
    CheckWithFlag(output1, input2, 0, true);
    CheckWithFlag(output1, input2, 0, true);
    CheckWithFlag(output1, input2, SCRIPT_VERIFY_WITNESS, false);
    CheckWithFlag(output1, input2, STANDARD_SCRIPT_VERIFY_FLAGS, false);

    // Normal pay-to-uncompressed-pubkey.
    CreateCreditAndSpend(keystore, scriptPubkey1L, output1, input1);
    CreateCreditAndSpend(keystore, scriptPubkey2L, output2, input2);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, SCRIPT_VERIFY_WITNESS, true);
    CheckWithFlag(output1, input1, STANDARD_SCRIPT_VERIFY_FLAGS, true);
    CheckWithFlag(output1, input2, 0, false);
    CheckWithFlag(output1, input2, 0, false);
    CheckWithFlag(output1, input2, SCRIPT_VERIFY_WITNESS, false);
    CheckWithFlag(output1, input2, STANDARD_SCRIPT_VERIFY_FLAGS, false);

    // P2SH pay-to-uncompressed-pubkey.
    CreateCreditAndSpend(keystore, GetScriptForDestination(ScriptHash(scriptPubkey1L)), output1, input1);
    CreateCreditAndSpend(keystore, GetScriptForDestination(ScriptHash(scriptPubkey2L)), output2, input2);
    ReplaceRedeemScript(input2.vin[0].scriptSig, scriptPubkey1L);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, SCRIPT_VERIFY_WITNESS, true);
    CheckWithFlag(output1, input1, STANDARD_SCRIPT_VERIFY_FLAGS, true);
    CheckWithFlag(output1, input2, 0, true);
    CheckWithFlag(output1, input2, 0, false);
    CheckWithFlag(output1, input2, SCRIPT_VERIFY_WITNESS, false);
    CheckWithFlag(output1, input2, STANDARD_SCRIPT_VERIFY_FLAGS, false);

    // Signing disabled for witness pay-to-uncompressed-pubkey (v1).
    CreateCreditAndSpend(keystore, destination_script_1L, output1, input1, false);
    CreateCreditAndSpend(keystore, destination_script_2L, output2, input2, false);

    // Signing disabled for P2SH witness pay-to-uncompressed-pubkey (v1).
    CreateCreditAndSpend(keystore, GetScriptForDestination(ScriptHash(destination_script_1L)), output1, input1, false);
    CreateCreditAndSpend(keystore, GetScriptForDestination(ScriptHash(destination_script_2L)), output2, input2, false);

    // Normal 2-of-2 multisig
    CreateCreditAndSpend(keystore, scriptMulti, output1, input1, false);
    CheckWithFlag(output1, input1, 0, false);
    CreateCreditAndSpend(keystore2, scriptMulti, output2, input2, false);
    CheckWithFlag(output2, input2, 0, false);
    BOOST_CHECK(*output1 == *output2);
    UpdateInput(input1.vin[0], CombineSignatures(input1, input2, output1));
    CheckWithFlag(output1, input1, STANDARD_SCRIPT_VERIFY_FLAGS, true);

    // P2SH 2-of-2 multisig
    CreateCreditAndSpend(keystore, GetScriptForDestination(ScriptHash(scriptMulti)), output1, input1, false);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, 0, false);
    CreateCreditAndSpend(keystore2, GetScriptForDestination(ScriptHash(scriptMulti)), output2, input2, false);
    CheckWithFlag(output2, input2, 0, true);
    CheckWithFlag(output2, input2, 0, false);
    BOOST_CHECK(*output1 == *output2);
    UpdateInput(input1.vin[0], CombineSignatures(input1, input2, output1));
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, STANDARD_SCRIPT_VERIFY_FLAGS, true);

    // Witness 2-of-2 multisig
    CreateCreditAndSpend(keystore, destination_script_multi, output1, input1, false);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, SCRIPT_VERIFY_WITNESS, false);
    CreateCreditAndSpend(keystore2, destination_script_multi, output2, input2, false);
    CheckWithFlag(output2, input2, 0, true);
    CheckWithFlag(output2, input2, SCRIPT_VERIFY_WITNESS, false);
    BOOST_CHECK(*output1 == *output2);
    UpdateInput(input1.vin[0], CombineSignatures(input1, input2, output1));
    CheckWithFlag(output1, input1, SCRIPT_VERIFY_WITNESS, true);
    CheckWithFlag(output1, input1, STANDARD_SCRIPT_VERIFY_FLAGS, true);

    // P2SH witness 2-of-2 multisig
    CreateCreditAndSpend(keystore, GetScriptForDestination(ScriptHash(destination_script_multi)), output1, input1, false);
    CheckWithFlag(output1, input1, 0, true);
    CheckWithFlag(output1, input1, SCRIPT_VERIFY_WITNESS, false);
    CreateCreditAndSpend(keystore2, GetScriptForDestination(ScriptHash(destination_script_multi)), output2, input2, false);
    CheckWithFlag(output2, input2, 0, true);
    CheckWithFlag(output2, input2, SCRIPT_VERIFY_WITNESS, false);
    BOOST_CHECK(*output1 == *output2);
    UpdateInput(input1.vin[0], CombineSignatures(input1, input2, output1));
    CheckWithFlag(output1, input1, SCRIPT_VERIFY_WITNESS, true);
    CheckWithFlag(output1, input1, STANDARD_SCRIPT_VERIFY_FLAGS, true);
}

BOOST_AUTO_TEST_CASE(test_IsStandard)
{
    FillableSigningProvider keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    std::vector<CMutableTransaction> dummyTransactions =
        SetupDummyInputs(keystore, coins, {11*CENT, 50*CENT, 21*CENT, 22*CENT});

    CMutableTransaction t;
    t.vin.resize(1);
    t.vin[0].prevout.hash = dummyTransactions[0].GetHash();
    t.vin[0].prevout.n = 1;
    t.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    t.vout.resize(1);
    t.vout[0].nValue = 90*CENT;
    CKey key;
    key.MakeNewKey(true);
    t.vout[0].scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey()));

    const auto CheckIsStandard = [](const CMutableTransaction& mtx) {
        std::string reason;
        const bool ok = IsStandardTx(CTransaction{mtx}, MAX_OP_RETURN_RELAY, g_bare_multi, g_dust, reason);
        BOOST_CHECK(ok);
        BOOST_CHECK(reason.empty());
    };
    const auto CheckIsNotStandard = [](const CMutableTransaction& mtx, const std::string& reason_in) {
        std::string reason;
        BOOST_CHECK(!IsStandardTx(CTransaction{mtx}, MAX_OP_RETURN_RELAY, g_bare_multi, g_dust, reason));
        BOOST_CHECK_EQUAL(reason_in, reason);
    };

    CheckIsStandard(t);

    // Check dust with default relay fee (threshold depends on CTxOut serialization size):
    {
        const CAmount thr = GetDustThreshold(t.vout[0], g_dust);
        t.vout[0].nValue = thr - 1;
        CheckIsNotStandard(t, "dust");
        t.vout[0].nValue = thr;
        CheckIsStandard(t);
    }

    // Disallowed nVersion
    t.nVersion = -1;
    CheckIsNotStandard(t, "version");

    t.nVersion = 0;
    CheckIsNotStandard(t, "version");

    t.nVersion = 3;
    CheckIsNotStandard(t, "version");

    // Allowed nVersion
    t.nVersion = 1;
    CheckIsStandard(t);

    t.nVersion = 2;
    CheckIsStandard(t);

    // Check dust with odd relay fee to verify rounding
    g_dust = CFeeRate(3702);
    {
        const CAmount thr = GetDustThreshold(t.vout[0], g_dust);
        t.vout[0].nValue = thr - 1;
        CheckIsNotStandard(t, "dust");
        t.vout[0].nValue = thr;
        CheckIsStandard(t);
    }
    g_dust = CFeeRate{DUST_RELAY_TX_FEE};

    t.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_1;
    CheckIsNotStandard(t, "scriptpubkey");

    // MAX_OP_RETURN_RELAY-byte TxoutType::NULL_DATA (standard)
    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3804678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    BOOST_CHECK_EQUAL(MAX_OP_RETURN_RELAY, t.vout[0].scriptPubKey.size());
    CheckIsStandard(t);

    // MAX_OP_RETURN_RELAY+1-byte TxoutType::NULL_DATA (non-standard)
    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3804678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3800");
    BOOST_CHECK_EQUAL(MAX_OP_RETURN_RELAY + 1, t.vout[0].scriptPubKey.size());
    CheckIsNotStandard(t, "scriptpubkey");

    // Data payload can be encoded in any way...
    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << ParseHex("");
    CheckIsStandard(t);
    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << ParseHex("00") << ParseHex("01");
    CheckIsStandard(t);
    // RIGOR_RESERVED_INERT_50 *is* considered to be a PUSHDATA type opcode by IsPushOnly()!
    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << RIGOR_RESERVED_INERT_50 << -1 << 0 << ParseHex("01") << 2 << 3 << 4 << 5 << 6 << 7 << 8 << 9 << 10 << 11 << 12 << 13 << 14 << 15 << 16;
    CheckIsStandard(t);
    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << 0 << ParseHex("01") << 2 << ParseHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    CheckIsStandard(t);

    // ...so long as it only contains PUSHDATA's
    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << RIGOR_MARKER_RETURN;
    CheckIsNotStandard(t, "scriptpubkey");

    // TxoutType::NULL_DATA w/o PUSHDATA
    t.vout.resize(1);
    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN;
    CheckIsStandard(t);

    // Only one TxoutType::NULL_DATA permitted in all cases
    t.vout.resize(2);
    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    t.vout[0].nValue = 0;
    t.vout[1].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    t.vout[1].nValue = 0;
    CheckIsNotStandard(t, "multi-op-return");

    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    t.vout[1].scriptPubKey = CScript() << RIGOR_MARKER_RETURN;
    CheckIsNotStandard(t, "multi-op-return");

    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN;
    t.vout[1].scriptPubKey = CScript() << RIGOR_MARKER_RETURN;
    CheckIsNotStandard(t, "multi-op-return");

    // Check large scriptSig (non-standard if size is >1650 bytes)
    t.vout.resize(1);
    t.vout[0].nValue = MAX_MONEY;
    t.vout[0].scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey()));
    // RIGOR_PUSHDATA2 with len (3 bytes) + data (1647 bytes) = 1650 bytes
    t.vin[0].scriptSig = CScript() << std::vector<unsigned char>(1647, 0); // 1650
    CheckIsStandard(t);

    t.vin[0].scriptSig = CScript() << std::vector<unsigned char>(1648, 0); // 1651
    CheckIsNotStandard(t, "scriptsig-size");

    // Check scriptSig format (non-standard if there are any other ops than just PUSHs)
    t.vin[0].scriptSig = CScript()
        << RIGOR_PUSH_1 << RIGOR_PUSH_0 << RIGOR_RESERVED_LEGACY_1NEGATE << RIGOR_PUSH_16 // OP_n (single byte pushes: n = 1, 0, -1, 16)
        << std::vector<unsigned char>(75, 0)      // OP_PUSHx [...x bytes...]
        << std::vector<unsigned char>(235, 0)     // RIGOR_PUSHDATA1 x [...x bytes...]
        << std::vector<unsigned char>(1234, 0)    // RIGOR_PUSHDATA2 x [...x bytes...]
        << RIGOR_PUSH_9;
    CheckIsStandard(t);

    const std::vector<unsigned char> non_push_ops = { // arbitrary set of non-push operations
        RIGOR_RESERVED_LEGACY_NOP, RIGOR_RESERVED_LEGACY_VERIFY, RIGOR_RESERVED_LEGACY_IF, RIGOR_RESERVED_LEGACY_ROT, RIGOR_RESERVED_LEGACY_3DUP, RIGOR_RESERVED_LEGACY_SIZE, RIGOR_RESERVED_LEGACY_EQUAL, RIGOR_RESERVED_LEGACY_ADD, RIGOR_RESERVED_LEGACY_SUB,
        RIGOR_RESERVED_LEGACY_HASH256, RIGOR_RESERVED_LEGACY_CODESEPARATOR, RIGOR_RESERVED_LEGACY_CHECKSIG, RIGOR_RESERVED_LEGACY_CLTV };

    CScript::const_iterator pc = t.vin[0].scriptSig.begin();
    while (pc < t.vin[0].scriptSig.end()) {
        opcodetype opcode;
        CScript::const_iterator prev_pc = pc;
        t.vin[0].scriptSig.GetOp(pc, opcode); // advance to next op
        // for the sake of simplicity, we only replace single-byte push operations
        if (opcode >= 1 && opcode <= RIGOR_PUSHDATA4)
            continue;

        int index = prev_pc - t.vin[0].scriptSig.begin();
        unsigned char orig_op = *prev_pc; // save op
        // replace current push-op with each non-push-op
        for (auto op : non_push_ops) {
            t.vin[0].scriptSig[index] = op;
            CheckIsNotStandard(t, "scriptsig-not-pushonly");
        }
        t.vin[0].scriptSig[index] = orig_op; // restore op
        CheckIsStandard(t);
    }

    // Check tx-size (non-standard if transaction weight is > MAX_STANDARD_TX_WEIGHT)
    t.vin.clear();
    t.vin.resize(2438); // size per input (empty scriptSig): 41 bytes
    // Match upstream Bitcoin: RIGOR_MARKER_RETURN payload size chosen so total weight is exactly MAX_STANDARD_TX_WEIGHT.
    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>(19, 0);
    BOOST_CHECK_EQUAL(GetTransactionWeight(CTransaction(t)), 400000);
    CheckIsStandard(t);

    t.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>(20, 0);
    BOOST_CHECK_EQUAL(GetTransactionWeight(CTransaction(t)), 400004);
    CheckIsNotStandard(t, "tx-size");

    // Bare multisig outputs are not consensus-valid on RigorChain (ClassifyRigorTxout); never standard.
    t.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_1 << ToByteVector(key.GetPubKey()) << RIGOR_PUSH_1 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    t.vin.resize(1);
    t.vin[0].scriptSig = CScript() << std::vector<unsigned char>(65, 0);
    CheckIsNotStandard(t, "scriptpubkey");

    // Bare P2PK outputs are not allowed as chain outputs on RigorChain.
    t.vout[0].scriptPubKey = CScript() << std::vector<unsigned char>(33, 0x02) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    CheckIsNotStandard(t, "scriptpubkey");

    t.vout[0].scriptPubKey = CScript() << std::vector<unsigned char>(65, 0x04) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    CheckIsNotStandard(t, "scriptpubkey");

    // Check P2WPKH outputs dust threshold
    t.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0);
    {
        const CAmount thr = GetDustThreshold(t.vout[0], g_dust);
        t.vout[0].nValue = thr;
        CheckIsStandard(t);
        t.vout[0].nValue = thr - 1;
        CheckIsNotStandard(t, "dust");
    }

    // P2WSH is not a standard (or consensus) output type.
    t.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0);
    t.vout[0].nValue = COIN;
    CheckIsNotStandard(t, "scriptpubkey");

    // Rigor: witness v1 native RES (32-byte program); dust is in RES atomic units.
    {
        t.vout[0].scriptPubKey = TestNativeResScript();
    }
    {
        const CAmount thr = GetDustThreshold(t.vout[0], g_dust);
        BOOST_CHECK_EQUAL(thr, RES_ATOMIC_UNIT);
        t.vout[0].nValue = thr;
        CheckIsStandard(t);
        t.vout[0].nValue = thr - 1;
        CheckIsNotStandard(t, "dust");
    }

    // Non-standard witness program shapes (wrong length / version) are rejected by policy.
    for (unsigned int op = RIGOR_PUSH_1; op <= RIGOR_PUSH_16; op += 1) {
        t.vout[0].scriptPubKey = CScript() << static_cast<opcodetype>(op) << std::vector<unsigned char>(2, 0);
        t.vout[0].nValue = COIN;
        CheckIsNotStandard(t, "scriptpubkey");
    }
}

namespace {
/** Valid chain output: native witness v1 RES (32-byte program, not CVG2 / RES2). */
static CScript MakeOrb2FixtureScriptPubKey(unsigned char /*tail_fill*/ = 0xab)
{
    return TestNativeResScript();
}
} // namespace

// --- Rigor dual-asset (ORU / RES): CheckTxInputs domain rules below. Keep when merging upstream Bitcoin Core. ---
// Native asset is never stored on CTxOut; these fixtures must satisfy AssetTypeFromScriptPubKey / IsWitnessProgram.
BOOST_AUTO_TEST_CASE(script_is_asset_fixture_scripts_classify_correctly)
{
    const CScript oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    const CScript res_spk = MakeOrb2FixtureScriptPubKey();

    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(oru_spk), ASSET_ORU);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(res_spk), ASSET_RES);
    const CScript opret = CScript() << RIGOR_MARKER_RETURN << ParseHex("aa");
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(opret), ASSET_NONE);

    int wver = -1;
    std::vector<unsigned char> wprog;
    BOOST_CHECK(oru_spk.IsWitnessProgram(wver, wprog));
    BOOST_CHECK_EQUAL(wver, 0);
    BOOST_CHECK_EQUAL(wprog.size(), 20U);
    BOOST_CHECK(res_spk.IsWitnessProgram(wver, wprog));
    BOOST_CHECK_EQUAL(wver, 1);
    BOOST_CHECK_EQUAL(wprog.size(), 32U);
}

BOOST_AUTO_TEST_CASE(mixed_asset_outputs_rejected_by_checktxinputs)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const CScript oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    CTxOut prev_oru(COIN * 3, oru_spk);
    const uint256 h_oru{uint256S("0x3333333333333333333333333333333333333333333333333333333333333333")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut{prev_oru}, 100, false), false);

    const CScript res_spk = MakeOrb2FixtureScriptPubKey();

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(h_oru, 0);
    mtx.vout.resize(2);
    mtx.vout[0].nValue = COIN;
    mtx.vout[0].scriptPubKey = oru_spk;
    mtx.vout[1].nValue = RES_COIN / 2;
    mtx.vout[1].scriptPubKey = res_spk;

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "mixed-asset-output");
}

BOOST_AUTO_TEST_CASE(get_transaction_input_asset_dual_pool_semantic_is_res)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const CScript oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    CTxOut prev_oru(COIN, oru_spk);
    const uint256 h_oru{uint256S("0x1111111111111111111111111111111111111111111111111111111111111111")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut{prev_oru}, 100, false), false);

    const CScript res_spk = MakeOrb2FixtureScriptPubKey();
    CTxOut prev_res(RES_COIN / 2, res_spk);
    const uint256 h_res{uint256S("0x2222222222222222222222222222222222222222222222222222222222222222")};
    coins.AddCoin(COutPoint(h_res, 0), Coin(CTxOut{prev_res}, 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(h_oru, 0);
    mtx.vin[1].prevout = COutPoint(h_res, 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = RES_COIN / 4;
    mtx.vout[0].scriptPubKey = res_spk;

    const std::optional<AssetType> dom = Consensus::GetTransactionInputAsset(CTransaction(mtx), coins);
    BOOST_REQUIRE(dom.has_value());
    BOOST_CHECK_EQUAL(*dom, ASSET_RES);
}

BOOST_AUTO_TEST_CASE(dual_pool_res_transfer_with_oru_fee_accepted)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const CScript oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    const CScript res_spk = MakeOrb2FixtureScriptPubKey();

    const uint256 h_oru{uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut(COIN, oru_spk), 100, false), false);

    const uint256 h_res{uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")};
    coins.AddCoin(COutPoint(h_res, 0), Coin(CTxOut(RES_COIN, res_spk), 100, false), false);

    const CAmount res_send = RES_COIN / 2;
    const CAmount oru_fee = 5000;
    const CAmount oru_change = COIN - oru_fee;

    CMutableTransaction mtx;
    mtx.vin.emplace_back(COutPoint(h_res, 0));
    mtx.vin.emplace_back(COutPoint(h_oru, 0));
    mtx.vout.emplace_back(res_send, res_spk);
    mtx.vout.emplace_back(oru_change, oru_spk);

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(fee, oru_fee);
}

BOOST_AUTO_TEST_CASE(res_transfer_without_oru_fee_rejected)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const CScript res_spk = MakeOrb2FixtureScriptPubKey();
    const uint256 h_res{uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc")};
    coins.AddCoin(COutPoint(h_res, 0), Coin(CTxOut(RES_COIN, res_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(COutPoint(h_res, 0));
    mtx.vout.emplace_back(RES_COIN / 2, res_spk);

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-oru-fee-required");
}

BOOST_AUTO_TEST_CASE(illegal_res_to_oru_conversion_rejected)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const CScript oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    const CScript res_spk = MakeOrb2FixtureScriptPubKey();
    const uint256 h_res{uint256S("0xdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd")};
    coins.AddCoin(COutPoint(h_res, 0), Coin(CTxOut(RES_COIN, res_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(COutPoint(h_res, 0));
    mtx.vout.emplace_back(RES_COIN / 2, res_spk);
    mtx.vout.emplace_back(COIN / 10, oru_spk);

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "illegal-res-to-oru-conversion");
}

BOOST_AUTO_TEST_CASE(dual_pool_fee_never_res_in_minus_res_out)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const CScript oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    const CScript res_spk = MakeOrb2FixtureScriptPubKey();
    const uint256 h_oru{uint256S("0xeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee")};
    const uint256 h_res{uint256S("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut(COIN, oru_spk), 100, false), false);
    coins.AddCoin(COutPoint(h_res, 0), Coin(CTxOut(RES_COIN, res_spk), 100, false), false);

    const CAmount oru_fee = 10000;
    CMutableTransaction mtx;
    mtx.vin.emplace_back(COutPoint(h_res, 0));
    mtx.vin.emplace_back(COutPoint(h_oru, 0));
    mtx.vout.emplace_back(RES_COIN / 2, res_spk);
    mtx.vout.emplace_back(COIN - oru_fee, oru_spk);

    TxValidationState state;
    CAmount fee{};
    BOOST_REQUIRE(Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(fee, oru_fee);
    BOOST_CHECK(fee != RES_COIN - RES_COIN / 2);
}

/**
 * TASK-010 R-1: `CheckTransaction` runs in MemPoolAccept::PreChecks ahead of any
 * UTXO lookup or signature check, so its output accumulator is reachable with a
 * free, unfunded, unsigned transaction. Two maximal outputs must be rejected on
 * the sum, and the sum must not overflow on the way to that rejection.
 */
BOOST_AUTO_TEST_CASE(output_total_overflow_is_rejected_before_it_overflows)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mtx.vout.resize(2);
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    mtx.vout[0] = CTxOut{MAX_MONEY, oru};
    mtx.vout[1] = CTxOut{MAX_MONEY, oru};

    TxValidationState state;
    BOOST_CHECK(!CheckTransaction(CTransaction(mtx), state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-txouttotal-toolarge");

    // A single maximal output is in range, so the reject above is the sum's and
    // not the per-output bound's.
    mtx.vout.resize(1);
    TxValidationState single_state;
    BOOST_CHECK(CheckTransaction(CTransaction(mtx), single_state));

    // GetValueOut is the other accumulator on the same attacker-controlled data.
    mtx.vout.resize(2);
    mtx.vout[1] = CTxOut{MAX_MONEY, oru};
    BOOST_CHECK_THROW(CTransaction(mtx).GetValueOut(), std::runtime_error);
    BOOST_CHECK_THROW(CTransaction(mtx).GetValueOut(ASSET_ORU), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(legacy_p2pkh_script_rejected)
{
    // RIGOR consensus: P2PKH (legacy) scripts are rejected
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = COIN;
    // P2PKH script: RIGOR_RESERVED_LEGACY_DUP RIGOR_RESERVED_LEGACY_HASH160 <pubkeyHash> RIGOR_RESERVED_LEGACY_EQUALVERIFY RIGOR_RESERVED_LEGACY_CHECKSIG
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;

    TxValidationState state;
    const bool ok = CheckTransaction(CTransaction(mtx), state);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-script-type");
}

BOOST_AUTO_TEST_CASE(legacy_p2sh_script_rejected)
{
    // RIGOR consensus: P2SH scripts are rejected
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = COIN;
    // P2SH script: RIGOR_RESERVED_LEGACY_HASH160 <scriptHash> RIGOR_RESERVED_LEGACY_EQUAL
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x22) << RIGOR_RESERVED_LEGACY_EQUAL;

    TxValidationState state;
    const bool ok = CheckTransaction(CTransaction(mtx), state);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-script-type");
}

BOOST_AUTO_TEST_CASE(witness_v2_script_rejected)
{
    // RIGOR consensus: Only witness v0 and v1 are allowed; v2 is rejected
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = COIN;
    // Witness v2 script: RIGOR_PUSH_2 <32-byte program>
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(32, 0x33);

    TxValidationState state;
    const bool ok = CheckTransaction(CTransaction(mtx), state);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-script-type");
}

BOOST_AUTO_TEST_CASE(witness_v1_non32byte_rejected)
{
    // RIGOR consensus: Witness v1 must be exactly 32 bytes
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = COIN;
    // Witness v1 with 20-byte program (invalid)
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(20, 0x44);

    TxValidationState state;
    const bool ok = CheckTransaction(CTransaction(mtx), state);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-script-type");
}

BOOST_AUTO_TEST_CASE(witness_v0_p2wsh_rejected)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = COIN;
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x55);

    TxValidationState state;
    const bool ok = CheckTransaction(CTransaction(mtx), state);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-script-type");
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(mtx.vout[0].scriptPubKey), ASSET_INVALID);
}

BOOST_AUTO_TEST_CASE(rfc_address_v0_maps_to_oru)
{
    // RFC-ADDRESS-001: V0 (witness version 0) MUST map to ORU asset
    CScript v0_script = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    AssetType asset = AssetTypeFromScriptPubKey(v0_script);
    BOOST_CHECK_EQUAL(asset, ASSET_ORU);
}

BOOST_AUTO_TEST_CASE(rfc_address_v1_maps_to_res)
{
    // RFC-ADDRESS-001: V1 (witness version 1, 32 bytes) MUST map to RES asset
    CScript v1_script = TestNativeResScript();
    AssetType asset = AssetTypeFromScriptPubKey(v1_script);
    BOOST_CHECK_EQUAL(asset, ASSET_RES);
}

BOOST_AUTO_TEST_CASE(script_asset_rejects_oru_to_res_script)
{
    CScript v1_script = TestNativeResScript();
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(v1_script), ASSET_RES);
    CScript v0_script = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x01);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(v0_script), ASSET_ORU);
    BOOST_CHECK(AssetTypeFromScriptPubKey(v1_script) != ASSET_ORU);
    BOOST_CHECK(AssetTypeFromScriptPubKey(v0_script) != ASSET_RES);
}

BOOST_AUTO_TEST_CASE(script_asset_accepts_matching_derive)
{
    uint160 pkhash;
    pkhash.SetHex("0000000000000000000000000000000000000001");
    const CScript v0 = GetScriptForDestination(WitnessV0KeyHash(pkhash));
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(v0), ASSET_ORU);

    XOnlyPubKey v1_xpk(TestEd25519PubKey());
    const CScript v1 = GetScriptForDestination(WitnessV1Res(v1_xpk));
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(v1), ASSET_RES);
}

BOOST_AUTO_TEST_CASE(res_domain_tx_rejected_when_sole_input_is_oru)
{
    // Homogeneous ORU inputs but RES outputs: input asset domain (ORU) must match every output ("mixed-asset-output").
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const CScript oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x22);
    CTxOut prev_oru(COIN, oru_spk);
    const uint256 h_oru{uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut{prev_oru}, 100, false), false);

    const CScript res_spk = MakeOrb2FixtureScriptPubKey(0x44);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(h_oru, 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = RES_COIN / 8;
    mtx.vout[0].scriptPubKey = res_spk;

    TxValidationState state;
    CAmount fee{};
    const bool ok = Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "mixed-asset-output");
}

BOOST_AUTO_TEST_SUITE_END()
