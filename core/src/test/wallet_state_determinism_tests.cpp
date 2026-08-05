// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <key.h>
#include <key_io.h>
#include <kernel/chain.h>
#include <primitives/transaction.h>
#include <script/descriptor.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <sync.h>
#include <test/util/setup_common.h>
#include <txmempool.h>
#include <uint256.h>
#include <validation.h>
#include <wallet/receive.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>

#include <boost/test/unit_test.hpp>

using wallet::AssetBalanceDetail;
using wallet::CWallet;
using wallet::CreateMockWalletDatabase;
using wallet::DatabaseOptions;
using wallet::DBErrors;
using wallet::DuplicateMockDatabase;
using wallet::GetAssetBalance;
using wallet::TxStateInactive;
using wallet::WalletDescriptor;

namespace {

void InitDescriptorWalletWithKey(TestingSetup& setup, CWallet& w)
{
    CKey key;
    key.MakeNewKey(true);
    LOCK2(w.cs_wallet, Assert(setup.m_node.chainman)->GetMutex());
    w.SetWalletFlag(wallet::WALLET_FLAG_DESCRIPTORS);
    w.SetLastBlockProcessed(setup.m_node.chainman->ActiveHeight(), setup.m_node.chainman->ActiveTip()->GetBlockHash());
    w.SetupDescriptorScriptPubKeyMans();
    FlatSigningProvider provider;
    std::string error;
    std::unique_ptr<Descriptor> desc = Parse("combo(" + EncodeSecret(key) + ")", provider, error, /*require_checksum=*/false);
    BOOST_REQUIRE_MESSAGE(desc, error);
    WalletDescriptor w_desc(std::move(desc), 0, 0, 1, 1);
    BOOST_REQUIRE(w.AddWalletDescriptor(w_desc, provider, "", false));
    BOOST_REQUIRE(w.TopUpKeyPool(128));
}

void InitDescriptorWalletOnChain(TestChain100Setup& setup, CWallet& w)
{
    LOCK2(w.cs_wallet, Assert(setup.m_node.chainman)->GetMutex());
    w.SetWalletFlag(wallet::WALLET_FLAG_DESCRIPTORS);
    w.SetLastBlockProcessed(setup.m_node.chainman->ActiveHeight(), setup.m_node.chainman->ActiveTip()->GetBlockHash());
    w.SetupDescriptorScriptPubKeyMans();
    FlatSigningProvider provider;
    std::string error;
    std::unique_ptr<Descriptor> desc = Parse("combo(" + EncodeSecret(setup.coinbaseKey) + ")", provider, error, /*require_checksum=*/false);
    BOOST_REQUIRE_MESSAGE(desc, error);
    WalletDescriptor w_desc(std::move(desc), 0, 0, 1, 1);
    BOOST_REQUIRE(w.AddWalletDescriptor(w_desc, provider, "", false));
    BOOST_REQUIRE(w.TopUpKeyPool(128));
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(wallet_state_determinism_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(abandontransaction_bumps_wallet_state_revision)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWalletWithKey(*this, w);
    const auto dest = w.GetNewDestination(OutputType::BECH32, "");
    BOOST_REQUIRE(dest);
    CMutableTransaction mtx;
    mtx.vout.push_back({COIN, GetScriptForDestination(*dest)});
    mtx.vin.push_back(CTxIn{g_insecure_rand_ctx.rand256(), 0});
    const uint256 txid = w.AddToWallet(MakeTransactionRef(mtx), TxStateInactive{})->GetHash();

    const uint64_t rev_before = w.GetWalletStateRevision();
    {
        LOCK(w.cs_wallet);
        BOOST_CHECK(w.AbandonTransaction(txid));
    }
    BOOST_CHECK_GT(w.GetWalletStateRevision(), rev_before);
}

BOOST_AUTO_TEST_CASE(mempool_callbacks_bump_revision_when_tracked)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWalletWithKey(*this, w);
    const auto dest = w.GetNewDestination(OutputType::BECH32, "");
    BOOST_REQUIRE(dest);
    CMutableTransaction mtx;
    mtx.vout.push_back({COIN, GetScriptForDestination(*dest)});
    mtx.vin.push_back(CTxIn{g_insecure_rand_ctx.rand256(), 0});
    w.AddToWallet(MakeTransactionRef(mtx), TxStateInactive{});

    const uint64_t r0 = w.GetWalletStateRevision();
    {
        LOCK(w.cs_wallet);
        w.transactionAddedToMempool(MakeTransactionRef(mtx));
    }
    BOOST_CHECK_GT(w.GetWalletStateRevision(), r0);

    const uint64_t r1 = w.GetWalletStateRevision();
    {
        LOCK(w.cs_wallet);
        w.transactionRemovedFromMempool(MakeTransactionRef(mtx), MemPoolRemovalReason::EXPIRY);
    }
    BOOST_CHECK_GT(w.GetWalletStateRevision(), r1);

    const uint64_t r2 = w.GetWalletStateRevision();
    {
        LOCK(w.cs_wallet);
        w.transactionRemovedFromMempool(MakeTransactionRef(mtx), MemPoolRemovalReason::CONFLICT);
    }
    BOOST_CHECK_GT(w.GetWalletStateRevision(), r2);
}

BOOST_AUTO_TEST_CASE(dual_native_snapshot_stable_under_single_cs_wallet_lock)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWalletWithKey(*this, w);
    const auto dest = w.GetNewDestination(OutputType::BECH32, "");
    BOOST_REQUIRE(dest);
    CMutableTransaction mtx;
    mtx.vout.push_back({COIN, GetScriptForDestination(*dest)});
    mtx.vin.push_back(CTxIn{g_insecure_rand_ctx.rand256(), 0});
    w.AddToWallet(MakeTransactionRef(mtx), TxStateInactive{});

    LOCK(w.cs_wallet);
    const uint256 h1 = w.GetLastBlockHash();
    const AssetBalanceDetail oru1 = GetAssetBalance(w, ASSET_ORU);
    const AssetBalanceDetail res1 = GetAssetBalance(w, ASSET_RES);
    const uint256 h2 = w.GetLastBlockHash();
    const AssetBalanceDetail oru2 = GetAssetBalance(w, ASSET_ORU);
    const AssetBalanceDetail res2 = GetAssetBalance(w, ASSET_RES);
    BOOST_CHECK_EQUAL(h1, h2);
    BOOST_CHECK_EQUAL(oru1.trusted, oru2.trusted);
    BOOST_CHECK_EQUAL(oru1.pending, oru2.pending);
    BOOST_CHECK_EQUAL(res1.trusted, res2.trusted);
    BOOST_CHECK_EQUAL(res1.pending, res2.pending);
}

BOOST_AUTO_TEST_CASE(duplicate_mock_database_preserves_wallet_tx_records)
{
    CWallet w1(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWalletWithKey(*this, w1);
    const auto dest = w1.GetNewDestination(OutputType::BECH32, "");
    BOOST_REQUIRE(dest);
    CMutableTransaction mtx;
    mtx.vout.push_back({2 * COIN, GetScriptForDestination(*dest)});
    mtx.vin.push_back(CTxIn{g_insecure_rand_ctx.rand256(), 0});
    w1.AddToWallet(MakeTransactionRef(mtx), TxStateInactive{});
    w1.Flush();

    DatabaseOptions dup_opts{};
    auto db2 = DuplicateMockDatabase(w1.GetDatabase(), dup_opts);
    CWallet w2(m_node.chain.get(), "", std::move(db2));
    BOOST_CHECK_EQUAL(w2.LoadWallet(), DBErrors::LOAD_OK);

    LOCK2(w1.cs_wallet, w2.cs_wallet);
    BOOST_CHECK_EQUAL(w1.mapWallet.size(), w2.mapWallet.size());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(wallet_state_determinism_chain_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(block_connected_bumps_wallet_state_revision)
{
    auto w = std::make_unique<CWallet>(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWalletOnChain(*this, *w);
    const uint64_t r0 = w->GetWalletStateRevision();
    const CBlock block = CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    const CBlockIndex* pindex = WITH_LOCK(::cs_main, return Assert(m_node.chainman)->ActiveTip());
    BOOST_REQUIRE(pindex);
    BOOST_CHECK_EQUAL(pindex->GetBlockHash(), block.GetHash());
    w->blockConnected(kernel::MakeBlockInfo(pindex, &block));
    BOOST_CHECK_GT(w->GetWalletStateRevision(), r0);
}

BOOST_AUTO_TEST_SUITE_END()
