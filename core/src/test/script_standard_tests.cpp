// Copyright (c) 2017-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <key_io.h>
#include <script/script.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(script_standard_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dest_default_is_no_dest)
{
    CTxDestination dest;
    BOOST_CHECK(!IsValidDestination(dest));
}

BOOST_AUTO_TEST_CASE(script_standard_Solver_success)
{
    CKey keys[3];
    CPubKey pubkeys[3];
    for (int i = 0; i < 3; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CScript s;
    std::vector<std::vector<unsigned char> > solutions;

    // Bare P2PK / P2PKH / P2SH / MULTISIG: not standard for coinbase outputs (IsStandard), but Solver
    // must classify them for nested redeem/witness scripts and descriptor inference (Bitcoin-compatible).
    s.clear();
    s << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::PUBKEY);

    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::PUBKEYHASH);

    CScript redeemScript(s);
    s.clear();
    s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::SCRIPTHASH);

    s.clear();
    s << OP_1 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::MULTISIG);

    s.clear();
    s << OP_2 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        ToByteVector(pubkeys[2]) <<
        OP_3 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::MULTISIG);

    // TxoutType::NULL_DATA
    s.clear();
    s << OP_RETURN <<
        std::vector<unsigned char>({0}) <<
        std::vector<unsigned char>({75}) <<
        std::vector<unsigned char>({255});
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NULL_DATA);
    BOOST_CHECK_EQUAL(solutions.size(), 0U);

    // TxoutType::WITNESS_V0_KEYHASH
    s.clear();
    s << OP_0 << ToByteVector(pubkeys[0].GetID());
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::WITNESS_V0_KEYHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1U);
    BOOST_CHECK(solutions[0] == ToByteVector(pubkeys[0].GetID()));

    // TxoutType::WITNESS_V0_SCRIPTHASH (32-byte program; arbitrary witness script — e.g. Ed25519 multisig — hashes here)
    uint256 scriptHash;
    CSHA256().Write(redeemScript.data(), redeemScript.size())
        .Finalize(scriptHash.begin());

    s.clear();
    s << OP_0 << ToByteVector(scriptHash);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::WITNESS_V0_SCRIPTHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1U);
    BOOST_CHECK(solutions[0] == ToByteVector(scriptHash));

    s.clear();
    s << OP_0 << ToByteVector(uint256::ONE);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::WITNESS_V0_SCRIPTHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1U);
    BOOST_CHECK(solutions[0] == ToByteVector(uint256::ONE));

    // TxoutType::WITNESS_V1_RES — CVG2 or ORB2 tag (32-byte program)
    {
        std::vector<unsigned char> prog(32, 0xab);
        prog[0] = 'C';
        prog[1] = 'V';
        prog[2] = 'G';
        prog[3] = '2';
        s.clear();
        s << OP_1 << prog;
        BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::WITNESS_V1_RES);
    }
    {
        std::vector<unsigned char> prog(32, 0xcd);
        prog[0] = 'O';
        prog[1] = 'R';
        prog[2] = 'B';
        prog[3] = '2';
        s.clear();
        s << OP_1 << prog;
        BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::WITNESS_V1_RES);
    }
    // Any witness v1 with 32-byte program is classified as WITNESS_V1_RES in Rigor (CVG2/ORB2 vs other is not a Solver distinction).
    s.clear();
    s << OP_1 << ToByteVector(uint256::ZERO);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::WITNESS_V1_RES);

    // Unknown witness version / length (not v0/v1 RES): NONSTANDARD
    s.clear();
    s << OP_16 << ToByteVector(uint256::ONE);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::NONSTANDARD
    s.clear();
    s << OP_9 << OP_ADD << OP_11 << OP_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);
}

BOOST_AUTO_TEST_CASE(script_standard_Solver_failure)
{
    CKey key;
    CPubKey pubkey;
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    CScript s;
    std::vector<std::vector<unsigned char> > solutions;

    // TxoutType::PUBKEY with incorrectly sized pubkey
    s.clear();
    s << std::vector<unsigned char>(30, 0x01) << OP_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::PUBKEYHASH with incorrectly sized key hash
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkey) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::SCRIPTHASH with incorrectly sized script hash
    s.clear();
    s << OP_HASH160 << std::vector<unsigned char>(21, 0x01) << OP_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::MULTISIG 0/2
    s.clear();
    s << OP_0 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::MULTISIG 2/1
    s.clear();
    s << OP_2 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::MULTISIG n = 2 with 1 pubkey
    s.clear();
    s << OP_1 << ToByteVector(pubkey) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::MULTISIG n = 1 with 0 pubkeys
    s.clear();
    s << OP_1 << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::NULL_DATA with other opcodes
    s.clear();
    s << OP_RETURN << std::vector<unsigned char>({75}) << OP_ADD;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::WITNESS_UNKNOWN with incorrect program size
    s.clear();
    s << OP_0 << std::vector<unsigned char>(19, 0x01);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);
}

BOOST_AUTO_TEST_CASE(script_standard_ExtractDestination)
{
    CKey key;
    CPubKey pubkey;
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    CScript s;
    CTxDestination address;

    // Bare P2PK / P2PKH / P2SH: Solver classifies them; ExtractDestination maps to PKHash / ScriptHash.
    s.clear();
    s << ToByteVector(pubkey) << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(std::holds_alternative<PKHash>(address));

    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(std::holds_alternative<PKHash>(address));

    CScript redeemScript(s);
    s.clear();
    s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(std::holds_alternative<ScriptHash>(address));

    // TxoutType::MULTISIG
    s.clear();
    s << OP_1 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK(!ExtractDestination(s, address));

    // TxoutType::NULL_DATA
    s.clear();
    s << OP_RETURN << std::vector<unsigned char>({75});
    BOOST_CHECK(!ExtractDestination(s, address));

    // TxoutType::WITNESS_V0_KEYHASH
    s.clear();
    s << OP_0 << ToByteVector(pubkey.GetID());
    BOOST_CHECK(ExtractDestination(s, address));
    WitnessV0KeyHash keyhash;
    CHash160().Write(pubkey).Finalize(keyhash);
    BOOST_CHECK(std::get<WitnessV0KeyHash>(address) == keyhash);

    // TxoutType::WITNESS_V0_SCRIPTHASH
    s.clear();
    WitnessV0ScriptHash scripthash;
    CSHA256().Write(redeemScript.data(), redeemScript.size()).Finalize(scripthash.begin());
    s << OP_0 << ToByteVector(scripthash);
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(std::get<WitnessV0ScriptHash>(address) == scripthash);

    // v1 program with wrong length for WITNESS_V1_RES (not 32 bytes): NONSTANDARD
    s.clear();
    s << OP_1 << ToByteVector(pubkey);
    BOOST_CHECK(!ExtractDestination(s, address));
}

BOOST_AUTO_TEST_CASE(script_standard_GetScriptFor_)
{
    CKey keys[3];
    CPubKey pubkeys[3];
    for (int i = 0; i < 3; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CScript expected, result;

    // PKHash
    expected.clear();
    expected << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    result = GetScriptForDestination(PKHash(pubkeys[0]));
    BOOST_CHECK(result == expected);

    // CScriptID
    CScript redeemScript(result);
    expected.clear();
    expected << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    result = GetScriptForDestination(ScriptHash(redeemScript));
    BOOST_CHECK(result == expected);

    // CNoDestination
    expected.clear();
    result = GetScriptForDestination(CNoDestination());
    BOOST_CHECK(result == expected);

    // GetScriptForRawPubKey
    expected.clear();
    expected << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    result = GetScriptForRawPubKey(pubkeys[0]);
    BOOST_CHECK(result == expected);

    // GetScriptForMultisig
    expected.clear();
    expected << OP_2 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        ToByteVector(pubkeys[2]) <<
        OP_3 << OP_CHECKMULTISIG;
    result = GetScriptForMultisig(2, std::vector<CPubKey>(pubkeys, pubkeys + 3));
    BOOST_CHECK(result == expected);

    // WitnessV0KeyHash
    expected.clear();
    expected << OP_0 << ToByteVector(pubkeys[0].GetID());
    result = GetScriptForDestination(WitnessV0KeyHash(Hash160(ToByteVector(pubkeys[0]))));
    BOOST_CHECK(result == expected);
    result = GetScriptForDestination(WitnessV0KeyHash(pubkeys[0].GetID()));
    BOOST_CHECK(result == expected);

    // WitnessV0ScriptHash (multisig)
    CScript witnessScript;
    witnessScript << OP_1 << ToByteVector(pubkeys[0]) << OP_1 << OP_CHECKMULTISIG;

    uint256 scriptHash;
    CSHA256().Write(witnessScript.data(), witnessScript.size())
        .Finalize(scriptHash.begin());

    expected.clear();
    expected << OP_0 << ToByteVector(scriptHash);
    result = GetScriptForDestination(WitnessV0ScriptHash(witnessScript));
    BOOST_CHECK(result == expected);
}

BOOST_AUTO_TEST_SUITE_END()
