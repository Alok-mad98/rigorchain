// Copyright (c) 2020-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <outputtype.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <wallet/receive.h>
#include <wallet/scriptpubkeyman.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>

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

BOOST_FIXTURE_TEST_CASE(descriptor_topup_skip_empty_and_with_db, TestingSetup)
{
    CWallet wallet(m_node.chain.get(), "", CreateMockWalletDatabase());
    {
        LOCK2(wallet.cs_wallet, Assert(m_node.chainman)->GetMutex());
        wallet.SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
        wallet.SetLastBlockProcessed(m_node.chainman->ActiveHeight(), m_node.chainman->ActiveTip()->GetBlockHash());
        wallet.SetupDescriptorScriptPubKeyMans();
    }

    LOCK(wallet.cs_wallet);
    auto* desc = dynamic_cast<DescriptorScriptPubKeyMan*>(wallet.GetScriptPubKeyMan(OutputType::BECH32, /*internal=*/false));
    BOOST_REQUIRE(desc);

    // SetupDescriptorGeneration already filled the keypool. A second TopUp
    // must not open a nested SQLite txn (would deadlock on the write semaphore).
    BOOST_CHECK(desc->TopUp());
    {
        WalletBatch batch(wallet.GetDatabase());
        BOOST_REQUIRE(batch.TxnBegin());
        BOOST_CHECK(desc->TopUp());
        BOOST_CHECK(desc->TopUpWithDB(batch));
        BOOST_CHECK(batch.TxnCommit());
    }

    const auto dest = wallet.GetNewDestination(OutputType::BECH32, "");
    BOOST_REQUIRE(dest);
    const CScript script = GetScriptForDestination(*dest);
    {
        WalletBatch batch(wallet.GetDatabase());
        BOOST_REQUIRE(batch.TxnBegin());
        const auto affected = desc->MarkUnusedAddresses(script);
        BOOST_CHECK(affected.empty());
        BOOST_CHECK(batch.TxnCommit());
    }

    const int32_t before = desc->GetEndRange();
    {
        WalletBatch batch(wallet.GetDatabase());
        BOOST_REQUIRE(batch.TxnBegin());
        BOOST_CHECK(desc->TopUpWithDB(batch, static_cast<unsigned int>(before + 50)));
        BOOST_CHECK(batch.TxnCommit());
    }
    BOOST_CHECK_GE(desc->GetEndRange(), before + 50);
}

BOOST_FIXTURE_TEST_CASE(dual_asset_balance_single_pass_matches, TestChain100Setup)
{
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);

    const DualAssetWalletBalances dual = GetDualAssetWalletBalances(*wallet, /*include_watch_only=*/false);
    const auto oru_conf = GetAssetBalance(*wallet, ASSET_ORU, /*min_depth=*/1);
    const auto oru_all = GetAssetBalance(*wallet, ASSET_ORU, /*min_depth=*/0);
    const auto res_conf = GetAssetBalance(*wallet, ASSET_RES, /*min_depth=*/1);
    const auto res_all = GetAssetBalance(*wallet, ASSET_RES, /*min_depth=*/0);
    BOOST_CHECK_EQUAL(dual.oru_trusted_min1, oru_conf.trusted);
    BOOST_CHECK_EQUAL(dual.oru_trusted_min0, oru_all.trusted);
    BOOST_CHECK_EQUAL(dual.oru_pending, oru_all.pending);
    BOOST_CHECK_EQUAL(dual.oru_immature, GetAssetImmatureBalance(*wallet, ASSET_ORU, ISMINE_SPENDABLE));
    BOOST_CHECK_EQUAL(dual.res_trusted_min1, res_conf.trusted);
    BOOST_CHECK_EQUAL(dual.res_trusted_min0, res_all.trusted);
    BOOST_CHECK_EQUAL(dual.res_pending, res_all.pending);
    BOOST_CHECK_EQUAL(dual.res_immature, GetAssetImmatureBalance(*wallet, ASSET_RES, ISMINE_SPENDABLE));

    const DualAssetWalletBalances dual2 = GetDualAssetWalletBalances(*wallet, /*include_watch_only=*/false);
    BOOST_CHECK_EQUAL(dual2.oru_trusted_min1, dual.oru_trusted_min1);
    BOOST_CHECK_EQUAL(dual2.oru_immature, dual.oru_immature);
    BOOST_CHECK_EQUAL(dual2.res_trusted_min1, dual.res_trusted_min1);
    BOOST_CHECK_EQUAL(dual2.res_immature, dual.res_immature);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet
