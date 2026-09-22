// Copyright (c) 2017-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <key_io.h>
#include <consensus/tx_check.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <test/util/script.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <map>

BOOST_FIXTURE_TEST_SUITE(script_standard_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dest_default_is_no_dest)
{
    CTxDestination dest;
    BOOST_CHECK(!IsValidDestination(dest));
}

BOOST_AUTO_TEST_CASE(cno_destination_strict_weak_ordering)
{
    // a < a must be false; otherwise std::map find/insert is undefined.
    BOOST_CHECK(!(CNoDestination() < CNoDestination()));
    BOOST_CHECK(CNoDestination() == CNoDestination());

    std::map<CTxDestination, int> m;
    m[CNoDestination()] = 1;
    BOOST_CHECK_EQUAL(m[CNoDestination()], 1);
    BOOST_CHECK(m.find(CNoDestination()) != m.end());
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

    // Bare P2PK / P2PKH / P2SH / MULTISIG are not RIGOR consensus types (Solver == ClassifyRigorTxout).
    s.clear();
    s << ToByteVector(pubkeys[0]) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    s.clear();
    s << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(pubkeys[0].GetID()) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    CScript redeemScript(s);
    s.clear();
    s << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(CScriptID(redeemScript)) << RIGOR_RESERVED_LEGACY_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    s.clear();
    s << RIGOR_PUSH_1 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    s.clear();
    s << RIGOR_PUSH_2 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        ToByteVector(pubkeys[2]) <<
        RIGOR_PUSH_3 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::NULL_DATA
    s.clear();
    s << RIGOR_MARKER_RETURN <<
        std::vector<unsigned char>({0}) <<
        std::vector<unsigned char>({75}) <<
        std::vector<unsigned char>({255});
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NULL_DATA);
    BOOST_CHECK_EQUAL(solutions.size(), 0U);

    // TxoutType::WITNESS_V0_KEYHASH
    s.clear();
    s << RIGOR_PUSH_0 << ToByteVector(pubkeys[0].GetID());
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::WITNESS_V0_KEYHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1U);
    BOOST_CHECK(solutions[0] == ToByteVector(pubkeys[0].GetID()));

    // TxoutType::WITNESS_V0_SCRIPTHASH is not a Rigor consensus type (P2WSH removed).
    uint256 scriptHash;
    CSHA256().Write(redeemScript.data(), redeemScript.size())
        .Finalize(scriptHash.begin());

    s.clear();
    s << RIGOR_PUSH_0 << ToByteVector(scriptHash);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    s.clear();
    s << RIGOR_PUSH_0 << ToByteVector(uint256::ONE);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::WITNESS_V1_RES — native 32-byte witness v1 only (not CVG2 / RES2).
    {
        std::vector<unsigned char> prog(32, 0xab);
        prog[0] = 'C';
        prog[1] = 'V';
        prog[2] = 'G';
        prog[3] = '2';
        s.clear();
        s << RIGOR_PUSH_1 << prog;
        BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);
    }
    {
        std::vector<unsigned char> prog(32, 0xab);
        prog[0] = 'R';
        prog[1] = 'E';
        prog[2] = 'S';
        prog[3] = '2';
        s.clear();
        s << RIGOR_PUSH_1 << prog;
        BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);
    }
    {
        std::vector<unsigned char> prog(32, 0xcd);
        prog[0] = 'O';
        prog[1] = 'R';
        prog[2] = 'B';
        prog[3] = '2';
        s.clear();
        s << RIGOR_PUSH_1 << prog;
        BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::WITNESS_V1_RES);
    }
    s.clear();
    s << RIGOR_PUSH_1 << TestEd25519PubKey();
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::WITNESS_V1_RES);

    s.clear();
    s << RIGOR_PUSH_1 << ToByteVector(uint256::ZERO);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::WITNESS_V1_RES);

    {
        const uint160 pkh;
        const CScript mint = GetResMintScript(/*maturity_height=*/150, pkh);
        BOOST_CHECK_EQUAL(Solver(mint, solutions), TxoutType::RES_MINT);
        BOOST_CHECK_EQUAL(solutions.size(), 1U);
        BOOST_CHECK_EQUAL(solutions[0].size(), 20U);
        BOOST_CHECK(IsResMintScript(mint));
        BOOST_CHECK_EQUAL(GetTxnOutputType(TxoutType::RES_MINT), std::string("res_mint"));

        const CScript conv = GetConvergeScript(pkh);
        BOOST_CHECK_EQUAL(Solver(conv, solutions), TxoutType::CONVERGE);
        BOOST_CHECK_EQUAL(solutions.size(), 1U);
        BOOST_CHECK_EQUAL(solutions[0].size(), 20U);
        BOOST_CHECK(IsConvergeScript(conv));
        BOOST_CHECK_EQUAL(GetTxnOutputType(TxoutType::CONVERGE), std::string("converge"));
        // Burn lock: classified as ORU, but not an address and not a UTXO.
        BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(conv), ASSET_ORU);
        CTxDestination dest;
        BOOST_CHECK(!ExtractDestination(conv, dest));
        BOOST_CHECK(!EntersUtxoSet(conv));
        BOOST_CHECK(!conv.IsUnspendable());

        // The retired OP_CVG_CLAIM shape (see script.h on 0xbc) has no classifier branch left.
        const CScript retired = CScript() << ToByteVector(pkh) << static_cast<opcodetype>(0xbc);
        BOOST_CHECK_EQUAL(Solver(retired, solutions), TxoutType::NONSTANDARD);
    }

    // Unknown witness version / length (not v0/v1 RES): NONSTANDARD
    s.clear();
    s << RIGOR_PUSH_16 << ToByteVector(uint256::ONE);
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::NONSTANDARD
    s.clear();
    s << RIGOR_PUSH_9 << RIGOR_RESERVED_LEGACY_ADD << RIGOR_PUSH_11 << RIGOR_RESERVED_LEGACY_EQUAL;
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
    s << std::vector<unsigned char>(30, 0x01) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::PUBKEYHASH with incorrectly sized key hash
    s.clear();
    s << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(pubkey) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::SCRIPTHASH with incorrectly sized script hash
    s.clear();
    s << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(21, 0x01) << RIGOR_RESERVED_LEGACY_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::MULTISIG 0/2
    s.clear();
    s << RIGOR_PUSH_0 << ToByteVector(pubkey) << RIGOR_PUSH_1 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::MULTISIG 2/1
    s.clear();
    s << RIGOR_PUSH_2 << ToByteVector(pubkey) << RIGOR_PUSH_1 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::MULTISIG n = 2 with 1 pubkey
    s.clear();
    s << RIGOR_PUSH_1 << ToByteVector(pubkey) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::MULTISIG n = 1 with 0 pubkeys
    s.clear();
    s << RIGOR_PUSH_1 << RIGOR_PUSH_1 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::NULL_DATA with other opcodes
    s.clear();
    s << RIGOR_MARKER_RETURN << std::vector<unsigned char>({75}) << RIGOR_RESERVED_LEGACY_ADD;
    BOOST_CHECK_EQUAL(Solver(s, solutions), TxoutType::NONSTANDARD);

    // TxoutType::WITNESS_UNKNOWN with incorrect program size
    s.clear();
    s << RIGOR_PUSH_0 << std::vector<unsigned char>(19, 0x01);
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

    // Bare P2PK / P2PKH / P2SH are classified by Solver but are not RIGOR destinations.
    s.clear();
    s << ToByteVector(pubkey) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK(!ExtractDestination(s, address));

    s.clear();
    s << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(pubkey.GetID()) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK(!ExtractDestination(s, address));

    CScript redeemScript(s);
    s.clear();
    s << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(CScriptID(redeemScript)) << RIGOR_RESERVED_LEGACY_EQUAL;
    BOOST_CHECK(!ExtractDestination(s, address));

    // TxoutType::MULTISIG
    s.clear();
    s << RIGOR_PUSH_1 << ToByteVector(pubkey) << RIGOR_PUSH_1 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK(!ExtractDestination(s, address));

    // TxoutType::NULL_DATA
    s.clear();
    s << RIGOR_MARKER_RETURN << std::vector<unsigned char>({75});
    BOOST_CHECK(!ExtractDestination(s, address));

    // TxoutType::WITNESS_V0_KEYHASH — HASH160 of the 32-byte Ed25519 payload, not the 0x00||pk wrapper.
    s.clear();
    s << RIGOR_PUSH_0 << ToByteVector(pubkey.GetID());
    BOOST_CHECK(ExtractDestination(s, address));
    WitnessV0KeyHash keyhash(pubkey.GetEd25519KeyHash());
    BOOST_CHECK(std::get<WitnessV0KeyHash>(address) == keyhash);

    // TxoutType::WITNESS_V0_SCRIPTHASH is not extracted (P2WSH removed).
    s.clear();
    WitnessV0ScriptHash scripthash;
    CSHA256().Write(redeemScript.data(), redeemScript.size()).Finalize(scripthash.begin());
    s << RIGOR_PUSH_0 << ToByteVector(scripthash);
    BOOST_CHECK(!ExtractDestination(s, address));

    // v1 program with wrong length for WITNESS_V1_RES (not 32 bytes): NONSTANDARD
    s.clear();
    s << RIGOR_PUSH_1 << ToByteVector(pubkey);
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

    // PKHash / ScriptHash / P2WSH constructors no longer emit Bitcoin scripts.
    result = GetScriptForDestination(PKHash(pubkeys[0]));
    BOOST_CHECK(result.empty());

    CScript redeemScript = CScript() << ToByteVector(pubkeys[0]) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    result = GetScriptForDestination(ScriptHash(redeemScript));
    BOOST_CHECK(result.empty());

    // CNoDestination
    expected.clear();
    result = GetScriptForDestination(CNoDestination());
    BOOST_CHECK(result == expected);

    // WitnessV0KeyHash
    expected.clear();
    expected << RIGOR_PUSH_0 << ToByteVector(pubkeys[0].GetID());
    result = GetScriptForDestination(WitnessV0KeyHash(pubkeys[0]));
    BOOST_CHECK(result == expected);
    result = GetScriptForDestination(WitnessV0KeyHash(pubkeys[0].GetID()));
    BOOST_CHECK(result == expected);

    // WitnessV0ScriptHash (multisig)
    CScript witnessScript;
    witnessScript << RIGOR_PUSH_1 << ToByteVector(pubkeys[0]) << RIGOR_PUSH_1 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    result = GetScriptForDestination(WitnessV0ScriptHash(witnessScript));
    BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(unknown_script_is_asset_invalid)
{
    CScript unknown;
    unknown << RIGOR_PUSH_1;
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(unknown), ASSET_INVALID);
    BOOST_CHECK(!IsRigorConstructibleScript(unknown));

    CScript p2wsh;
    p2wsh << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x01);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(p2wsh), ASSET_INVALID);
    BOOST_CHECK(!IsRigorConstructibleScript(p2wsh));

    CScript p2pkh;
    p2pkh << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(p2pkh), ASSET_INVALID);
    BOOST_CHECK(!IsRigorConstructibleScript(p2pkh));

    CScript opreturn;
    opreturn << RIGOR_MARKER_RETURN;
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(opreturn), ASSET_NONE);
    BOOST_CHECK(IsRigorConstructibleScript(opreturn));
    BOOST_CHECK(!Consensus::IsAllowedScript(unknown));
    BOOST_CHECK(!Consensus::IsAllowedScript(p2wsh));
    BOOST_CHECK(!Consensus::IsAllowedScript(p2pkh));
    BOOST_CHECK(!Consensus::IsAllowedScript(opreturn));
}

BOOST_AUTO_TEST_SUITE_END()
