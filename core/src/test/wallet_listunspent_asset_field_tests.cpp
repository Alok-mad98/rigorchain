// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// W1.1C regression test: listunspent emits an explicit per-UTXO "asset" field whose
// canonical strings are "ORU" | "RES" | "NONE". This test pins the contract.

#include <core_io.h>
#include <key.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

namespace {

// The production mapping, not a copy of it. This test used to mirror the ternary inlined in
// listunspent, which meant a new asset class could be added to one side and not the other
// while the test kept passing.
const char* AssetTagForListUnspent(AssetType a) { return AssetTypeToString(a); }

} // namespace

BOOST_FIXTURE_TEST_SUITE(wallet_listunspent_asset_field_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(asset_tag_canonical_strings_are_frozen)
{
    BOOST_CHECK_EQUAL(AssetTagForListUnspent(ASSET_ORU), std::string("ORU"));
    BOOST_CHECK_EQUAL(AssetTagForListUnspent(ASSET_RES), std::string("RES"));
    BOOST_CHECK_EQUAL(AssetTagForListUnspent(ASSET_NONE), std::string("NONE"));
}

BOOST_AUTO_TEST_CASE(p2wpkh_script_classifies_as_oru)
{
    CKey k;
    k.MakeNewKey(true);
    CScript p2wpkh = GetScriptForDestination(WitnessV0KeyHash(k.GetPubKey()));
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(p2wpkh), ASSET_ORU);
    BOOST_CHECK_EQUAL(AssetTagForListUnspent(AssetTypeFromScriptPubKey(p2wpkh)), std::string("ORU"));
}

BOOST_AUTO_TEST_CASE(witness_v1_native_res_classifies_as_res)
{
    // Native RES: OP_1 followed by a 32-byte program with no recognised prefix tag
    // (CVG2 / ORB2). Use a fixed, deterministic non-tag program.
    std::vector<unsigned char> program32(32, 0xAA);
    CScript res_spk;
    res_spk << OP_1 << program32;
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(res_spk), ASSET_RES);
    BOOST_CHECK_EQUAL(AssetTagForListUnspent(AssetTypeFromScriptPubKey(res_spk)), std::string("RES"));
}

BOOST_AUTO_TEST_SUITE_END()
