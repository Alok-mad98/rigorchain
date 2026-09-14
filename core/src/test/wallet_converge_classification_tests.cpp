// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Wallet classification of OP_CONVERGE burns, and the RFC-020 leftover-mint
// path: a floor-priced mint left in the mempool must be evicted on the next
// empty block instead of aborting CTxMemPool::check().

#include <consensus/amount.h>
#include <consensus/convergenceres.h>
#include <consensus/res_issuance.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <txmempool.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/receive.h>
#include <wallet/spend.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

namespace wallet {

BOOST_FIXTURE_TEST_SUITE(wallet_converge_classification_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(getamounts_skips_converge_and_empty_block_does_not_abort)
{
    // Coinbase maturity is COINBASE_MATURITY+1 confirmations. After the
    // 100-block fixture, ten more blocks yield ten mature 128-ORU coins
    // (1280 ORU) — enough for a multi-input whole-RES floor mint.
    mineBlocks(10);
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);

    const int confirm_height = WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain().Height()) + 1;
    const CAmount burn = Consensus::ResMinBurnForAtoms(Consensus::RES_COIN, confirm_height);
    BOOST_REQUIRE_GT(burn, 0);
    BOOST_REQUIRE(Consensus::CheckResMintArithmetic(Consensus::RES_COIN, burn, confirm_height));
    BOOST_REQUIRE(!Consensus::CheckResMintArithmetic(Consensus::RES_COIN, burn, confirm_height + 1));

    CTransactionRef mint_tx;
    {
        LOCK(wallet->cs_wallet);
        CCoinControl cc;
        cc.m_asset_type = ASSET_ORU;
        cc.m_feerate = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
        cc.fOverrideFeeRate = true;
        auto minted = CreateMintTransaction(*wallet, burn, /*change_pos=*/-1, cc, /*sign=*/true);
        BOOST_REQUIRE_MESSAGE(minted, util::ErrorString(minted).original);
        BOOST_CHECK_EQUAL(minted->res_out, Consensus::RES_COIN);
        BOOST_CHECK_EQUAL(minted->confirm_height, confirm_height);
        mint_tx = minted->txr.tx;
    }

    BOOST_REQUIRE(Consensus::IsConvergenceTx(*mint_tx));
    BOOST_CHECK_GE(mint_tx->vin.size(), 2U);

    wallet->CommitTransaction(mint_tx, {}, {});

    {
        LOCK(wallet->cs_wallet);
        const CWalletTx* wtx = wallet->GetWalletTx(mint_tx->GetHash());
        BOOST_REQUIRE(wtx);

        CAmount nFee = 0;
        std::list<COutputEntry> received;
        std::list<COutputEntry> sent;
        CachedTxGetAmounts(*wallet, *wtx, received, sent, nFee, ISMINE_ALL, /*include_change=*/true);

        int converge_outs = 0;
        for (const CTxOut& out : mint_tx->vout) {
            if (IsConvergenceScript(out.scriptPubKey)) {
                ++converge_outs;
                BOOST_CHECK(!EntersUtxoSet(out.scriptPubKey));
            }
        }
        BOOST_REQUIRE_EQUAL(converge_outs, 1);

        for (const COutputEntry& e : sent) {
            BOOST_CHECK_MESSAGE(!IsConvergenceScript(mint_tx->vout.at(static_cast<size_t>(e.vout)).scriptPubKey),
                                "GetAmounts must not list the OP_CONVERGE burn as a sent destination");
        }
        for (const COutputEntry& e : received) {
            BOOST_CHECK(!IsConvergenceScript(mint_tx->vout.at(static_cast<size_t>(e.vout)).scriptPubKey));
        }

        CoinFilterParams params;
        params.m_asset_type = ASSET_ORU;
        for (const COutput& c : AvailableCoins(*wallet, nullptr, std::nullopt, params).All()) {
            BOOST_CHECK(EntersUtxoSet(c.txout.scriptPubKey));
            BOOST_CHECK(!IsConvergenceScript(c.txout.scriptPubKey));
        }
    }

    if (!m_node.mempool->exists(GenTxid::Txid(mint_tx->GetHash()))) {
        LOCK(cs_main);
        const MempoolAcceptResult accepted = m_node.chainman->ProcessTransaction(mint_tx);
        BOOST_REQUIRE_EQUAL(accepted.m_result_type, MempoolAcceptResult::ResultType::VALID);
    }
    BOOST_REQUIRE(m_node.mempool->exists(GenTxid::Txid(mint_tx->GetHash())));

    // Same as generateblock addr '[]': empty body, mint stays behind at the old price.
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));

    BOOST_CHECK_MESSAGE(!m_node.mempool->exists(GenTxid::Txid(mint_tx->GetHash())),
                        "floor-priced mint must be evicted once the tip moves past its confirm height");
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace wallet
