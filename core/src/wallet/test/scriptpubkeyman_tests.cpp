// Copyright (c) 2020-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <wallet/scriptpubkeyman.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(scriptpubkeyman_tests, BasicTestingSetup)

// Test LegacyScriptPubKeyMan::CanProvide behavior, making sure it returns true
// for recognized scripts even when keys may not be available for signing.
BOOST_AUTO_TEST_CASE(CanProvide)
{
    // Set up wallet and keyman variables.
    CWallet wallet(m_node.chain.get(), "", CreateDummyWalletDatabase());
    LegacyScriptPubKeyMan& keyman = *wallet.GetOrCreateLegacyScriptPubKeyMan();

    CKey key;
    key.MakeNewKey(true);
    CScript wpkh = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey()));
    SignatureData data;

    BOOST_CHECK(!keyman.CanProvide(wpkh, data));
    BOOST_CHECK(keyman.AddKey(key));
    BOOST_CHECK(keyman.CanProvide(wpkh, data));
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet
