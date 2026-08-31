// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <policy/policy.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <test/util/setup_common.h>
#include <tinyformat.h>
#include <uint256.h>


#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(multisig_tests, BasicTestingSetup)

static CScript
sign_multisig(const CScript& scriptPubKey, const std::vector<CKey>& keys, const CTransaction& transaction, int whichIn)
{
    uint256 hash = SignatureHash(scriptPubKey, transaction, whichIn, 0, SigVersion::WITNESS_V0, TestSighashChainId());

    CScript result;
    result << RIGOR_PUSH_0; // CHECKMULTISIG bug workaround
    for (const CKey &key : keys)
    {
        std::vector<unsigned char> vchSig;
        BOOST_CHECK(key.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        result << vchSig;
    }
    return result;
}

BOOST_AUTO_TEST_CASE(multisig_verify)
{
    unsigned int flags = 0;

    ScriptError err;
    CKey key[4];
    CAmount amount = 0;
    for (int i = 0; i < 4; i++)
        key[i].MakeNewKey(true);

    CScript a_and_b;
    a_and_b << RIGOR_PUSH_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b << RIGOR_PUSH_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;

    CScript escrow;
    escrow << RIGOR_PUSH_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << RIGOR_PUSH_3 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;

    CMutableTransaction txFrom;  // Funding transaction
    txFrom.vout.resize(3);
    txFrom.vout[0].scriptPubKey = a_and_b;
    txFrom.vout[1].scriptPubKey = a_or_b;
    txFrom.vout[2].scriptPubKey = escrow;

    CMutableTransaction txTo[3]; // Spending transaction
    for (int i = 0; i < 3; i++)
    {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        txTo[i].vin[0].prevout.n = i;
        txTo[i].vin[0].prevout.hash = txFrom.GetHash();
        txTo[i].vout[0].nValue = 1;
    }

    std::vector<CKey> keys;
    CScript s;

    // Test a AND b:
    keys.assign(1,key[0]);
    keys.push_back(key[1]);
    s = sign_multisig(a_and_b, keys, CTransaction(txTo[0]), 0);
    BOOST_CHECK(VerifyRigorPredicate(s, a_and_b, nullptr, flags, MutableTransactionSignatureChecker(&txTo[0], 0, amount, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()), &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));

    for (int i = 0; i < 4; i++)
    {
        keys.assign(1,key[i]);
        s = sign_multisig(a_and_b, keys, CTransaction(txTo[0]), 0);
        BOOST_CHECK_MESSAGE(!VerifyRigorPredicate(s, a_and_b, nullptr, flags, MutableTransactionSignatureChecker(&txTo[0], 0, amount, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()), &err), strprintf("a&b 1: %d", i));
        BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_INVALID_STACK_OPERATION, ScriptErrorString(err));

        keys.assign(1,key[1]);
        keys.push_back(key[i]);
        s = sign_multisig(a_and_b, keys, CTransaction(txTo[0]), 0);
        BOOST_CHECK_MESSAGE(!VerifyRigorPredicate(s, a_and_b, nullptr, flags, MutableTransactionSignatureChecker(&txTo[0], 0, amount, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()), &err), strprintf("a&b 2: %d", i));
        BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
    }

    // Test a OR b:
    for (int i = 0; i < 4; i++)
    {
        keys.assign(1,key[i]);
        s = sign_multisig(a_or_b, keys, CTransaction(txTo[1]), 0);
        if (i == 0 || i == 1)
        {
            BOOST_CHECK_MESSAGE(VerifyRigorPredicate(s, a_or_b, nullptr, flags, MutableTransactionSignatureChecker(&txTo[1], 0, amount, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()), &err), strprintf("a|b: %d", i));
            BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
        }
        else
        {
            BOOST_CHECK_MESSAGE(!VerifyRigorPredicate(s, a_or_b, nullptr, flags, MutableTransactionSignatureChecker(&txTo[1], 0, amount, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()), &err), strprintf("a|b: %d", i));
            BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
        }
    }
    s.clear();
    s << RIGOR_PUSH_0 << RIGOR_PUSH_1;
    BOOST_CHECK(!VerifyRigorPredicate(s, a_or_b, nullptr, flags, MutableTransactionSignatureChecker(&txTo[1], 0, amount, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()), &err));
    BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_SIG_DER, ScriptErrorString(err));


    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            keys.assign(1,key[i]);
            keys.push_back(key[j]);
            s = sign_multisig(escrow, keys, CTransaction(txTo[2]), 0);
            if (i < j && i < 3 && j < 3)
            {
                BOOST_CHECK_MESSAGE(VerifyRigorPredicate(s, escrow, nullptr, flags, MutableTransactionSignatureChecker(&txTo[2], 0, amount, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()), &err), strprintf("escrow 1: %d %d", i, j));
                BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_OK, ScriptErrorString(err));
            }
            else
            {
                BOOST_CHECK_MESSAGE(!VerifyRigorPredicate(s, escrow, nullptr, flags, MutableTransactionSignatureChecker(&txTo[2], 0, amount, MissingDataBehavior::ASSERT_FAIL, TestSighashChainId()), &err), strprintf("escrow 2: %d %d", i, j));
                BOOST_CHECK_MESSAGE(err == SCRIPT_ERR_EVAL_FALSE, ScriptErrorString(err));
            }
        }
}

BOOST_AUTO_TEST_CASE(multisig_IsStandard)
{
    CKey key[4];
    for (int i = 0; i < 4; i++)
        key[i].MakeNewKey(true);

    const auto assert_not_standard = [](const CScript& spk) {
        TxoutType type;
        BOOST_CHECK(!::IsStandard(spk, std::nullopt, type));
        BOOST_CHECK_EQUAL(type, TxoutType::NONSTANDARD);
        std::vector<std::vector<unsigned char>> sols;
        BOOST_CHECK_EQUAL(Solver(spk, sols), TxoutType::MULTISIG);
    };

    CScript a_and_b;
    a_and_b << RIGOR_PUSH_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    assert_not_standard(a_and_b);

    CScript a_or_b;
    a_or_b  << RIGOR_PUSH_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    assert_not_standard(a_or_b);

    CScript escrow;
    escrow << RIGOR_PUSH_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << RIGOR_PUSH_3 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    assert_not_standard(escrow);

    CScript one_of_four;
    one_of_four << RIGOR_PUSH_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << ToByteVector(key[3].GetPubKey()) << RIGOR_PUSH_4 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    assert_not_standard(one_of_four);

    CScript malformed[6];
    malformed[0] << RIGOR_PUSH_3 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    malformed[1] << RIGOR_PUSH_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_PUSH_3 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    malformed[2] << RIGOR_PUSH_0 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    malformed[3] << RIGOR_PUSH_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_PUSH_0 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    malformed[4] << RIGOR_PUSH_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    malformed[5] << RIGOR_PUSH_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey());

    for (int i = 0; i < 6; i++) {
        TxoutType type;
        BOOST_CHECK(!::IsStandard(malformed[i], std::nullopt, type));
        BOOST_CHECK_EQUAL(type, TxoutType::NONSTANDARD);
    }
}

BOOST_AUTO_TEST_CASE(multisig_Sign)
{
    // Test SignSignature() (and therefore the version of Solver() that signs transactions)
    FillableSigningProvider keystore;
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(true);
        BOOST_CHECK(keystore.AddKey(key[i]));
    }

    CScript a_and_b;
    a_and_b << RIGOR_PUSH_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b  << RIGOR_PUSH_1 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;

    CScript escrow;
    escrow << RIGOR_PUSH_2 << ToByteVector(key[0].GetPubKey()) << ToByteVector(key[1].GetPubKey()) << ToByteVector(key[2].GetPubKey()) << RIGOR_PUSH_3 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;

    CMutableTransaction txFrom;  // Funding transaction
    txFrom.vout.resize(3);
    txFrom.vout[0].scriptPubKey = a_and_b;
    txFrom.vout[1].scriptPubKey = a_or_b;
    txFrom.vout[2].scriptPubKey = escrow;

    CMutableTransaction txTo[3]; // Spending transaction
    for (int i = 0; i < 3; i++)
    {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        txTo[i].vin[0].prevout.n = i;
        txTo[i].vin[0].prevout.hash = txFrom.GetHash();
        txTo[i].vout[0].nValue = 1;
    }

    for (int i = 0; i < 3; i++)
    {
        SignatureData empty;
        BOOST_CHECK_MESSAGE(SignSignature(keystore, CTransaction(txFrom), txTo[i], 0, TestSighashChainId(), empty), strprintf("SignSignature %d", i));
    }
}


BOOST_AUTO_TEST_SUITE_END()
