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
#include <wallet/mint_height_pad.h>
#include <wallet/spend.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

namespace wallet {

BOOST_FIXTURE_TEST_SUITE(wallet_converge_classification_tests, TestChain100Setup)

// Test C: old exact-fit (tip+1, height_pad=0) is evicted by one empty block.
// Proves consensus still uses the actual confirming height — this is not a
// consensus change.
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
        auto minted = CreateMintTransaction(*wallet, burn, /*change_pos=*/-1, cc, /*sign=*/true, /*height_pad=*/0);
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

        CAmount burn_listed = 0;
        for (const COutputEntry& e : sent) {
            const CScript& spk = mint_tx->vout.at(static_cast<size_t>(e.vout)).scriptPubKey;
            if (IsConvergenceScript(spk)) {
                burn_listed += e.amount;
                BOOST_CHECK_MESSAGE(!IsValidDestination(e.destination),
                                    "OP_CONVERGE burn must not decode as a spendable address");
            }
        }
        BOOST_CHECK_EQUAL(burn_listed, burn);
        BOOST_CHECK_GE(sent.size(), 1U);

        for (const COutputEntry& e : received) {
            BOOST_CHECK(!IsConvergenceScript(mint_tx->vout.at(static_cast<size_t>(e.vout)).scriptPubKey));
        }

        const CAmount oru_debit = CachedTxGetDebitForAsset(*wallet, *wtx, ASSET_ORU, ISMINE_ALL);
        const CAmount oru_credit = CachedTxGetCreditForAsset(*wallet, *wtx, ASSET_ORU, ISMINE_ALL);
        const CAmount oru_available = CachedTxGetAvailableCredit(*wallet, *wtx, ISMINE_SPENDABLE);
        BOOST_CHECK(wallet->IsTxImmatureResMint(*wtx));
        BOOST_CHECK_EQUAL(oru_available, oru_credit);
        BOOST_CHECK_GE(nFee, 0);
        BOOST_CHECK_LT(nFee, burn);
        // I = C + B + F. Burn is not credit and not network fee.
        BOOST_CHECK_EQUAL(oru_debit, oru_credit + burn + nFee);

        for (const CTxOut& out : mint_tx->vout) {
            if (IsConvergenceScript(out.scriptPubKey)) {
                BOOST_CHECK_EQUAL(wallet->IsMine(out), ISMINE_NO);
                BOOST_CHECK_EQUAL(OutputGetCredit(*wallet, out, ISMINE_ALL), 0);
            }
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

// Test A: default pad is 1024; target_height = current_height + 1024;
// several empty blocks later the mint is still consensus-valid.
BOOST_AUTO_TEST_CASE(test_a_default_pad_survives_empty_blocks)
{
    BOOST_CHECK_EQUAL(CONVERGENCE_HEIGHT_PAD, 1024);
    BOOST_CHECK_EQUAL(DEFAULT_CONVERGENCE_HEIGHT_PAD, 1024);
    BOOST_CHECK_EQUAL(ConvergenceTargetHeight(1000, CONVERGENCE_HEIGHT_PAD), 2024);

    mineBlocks(10);
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);

    const int current_height = WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain().Height());
    const int target_height = ConvergenceTargetHeight(current_height);
    BOOST_CHECK_EQUAL(target_height, current_height + CONVERGENCE_HEIGHT_PAD);

    const CAmount burn = Consensus::ResMinBurnForAtoms(Consensus::RES_COIN, target_height);
    BOOST_REQUIRE_GT(burn, 0);

    CTransactionRef mint_tx;
    {
        LOCK(wallet->cs_wallet);
        CCoinControl cc;
        cc.m_asset_type = ASSET_ORU;
        cc.m_feerate = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
        cc.fOverrideFeeRate = true;
        auto minted = CreateMintTransaction(*wallet, burn, /*change_pos=*/-1, cc, /*sign=*/true);
        BOOST_REQUIRE_MESSAGE(minted, util::ErrorString(minted).original);
        BOOST_CHECK_EQUAL(minted->current_height, current_height);
        BOOST_CHECK_EQUAL(minted->height_pad, CONVERGENCE_HEIGHT_PAD);
        BOOST_CHECK_EQUAL(minted->target_height, target_height);
        BOOST_CHECK_EQUAL(minted->destroy_cost_height, target_height);
        BOOST_CHECK_EQUAL(minted->res_out, Consensus::RES_COIN);
        mint_tx = minted->txr.tx;
    }

    wallet->CommitTransaction(mint_tx, {}, {});
    if (!m_node.mempool->exists(GenTxid::Txid(mint_tx->GetHash()))) {
        LOCK(cs_main);
        const MempoolAcceptResult accepted = m_node.chainman->ProcessTransaction(mint_tx);
        BOOST_REQUIRE_EQUAL(accepted.m_result_type, MempoolAcceptResult::ResultType::VALID);
    }
    BOOST_REQUIRE(m_node.mempool->exists(GenTxid::Txid(mint_tx->GetHash())));

    constexpr int empty_blocks = 8;
    for (int i = 0; i < empty_blocks; ++i) {
        CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
        BOOST_CHECK_MESSAGE(m_node.mempool->exists(GenTxid::Txid(mint_tx->GetHash())),
                            "default-pad mint must survive empty blocks inside the 1024-block margin");
    }
    const int spendheight = current_height + empty_blocks + 1;
    BOOST_CHECK(Consensus::CheckResMintArithmetic(Consensus::RES_COIN, burn, spendheight));
}

// Test B: the same constructed transaction is judged only by the actual
// confirming height. Validation is unchanged.
BOOST_AUTO_TEST_CASE(test_b_consensus_uses_actual_confirming_height)
{
    mineBlocks(10);
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);

    const int current_height = WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain().Height());
    const int target_height = ConvergenceTargetHeight(current_height);
    const CAmount burn = Consensus::ResMinBurnForAtoms(Consensus::RES_COIN, target_height);

    CAmount res_out = 0;
    {
        LOCK(wallet->cs_wallet);
        CCoinControl cc;
        cc.m_asset_type = ASSET_ORU;
        cc.m_feerate = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
        cc.fOverrideFeeRate = true;
        auto minted = CreateMintTransaction(*wallet, burn, /*change_pos=*/-1, cc, /*sign=*/true);
        BOOST_REQUIRE_MESSAGE(minted, util::ErrorString(minted).original);
        res_out = minted->res_out;
        BOOST_CHECK_EQUAL(res_out, Consensus::RES_COIN);
    }

    BOOST_CHECK(Consensus::CheckResMintArithmetic(res_out, burn, current_height));
    BOOST_CHECK(Consensus::CheckResMintArithmetic(res_out, burn, current_height + 1));
    BOOST_CHECK(Consensus::CheckResMintArithmetic(res_out, burn, target_height));
    BOOST_CHECK(!Consensus::CheckResMintArithmetic(res_out, burn, target_height + 1));
}

// Test D: new wallet construction reports height_pad and stays valid under
// mempool delay (empty blocks before the confirming height).
BOOST_AUTO_TEST_CASE(test_d_wallet_reports_height_pad_and_survives_delay)
{
    mineBlocks(10);
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);

    const int current_height = WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain().Height());
    constexpr int pad = 2;
    const int target_height = ConvergenceTargetHeight(current_height, pad);
    BOOST_CHECK_EQUAL(target_height, current_height + pad);
    const CAmount burn = Consensus::ResMinBurnForAtoms(Consensus::RES_COIN, target_height);

    CTransactionRef mint_tx;
    {
        LOCK(wallet->cs_wallet);
        CCoinControl cc;
        cc.m_asset_type = ASSET_ORU;
        cc.m_feerate = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
        cc.fOverrideFeeRate = true;
        auto minted = CreateMintTransaction(*wallet, burn, /*change_pos=*/-1, cc, /*sign=*/true, pad);
        BOOST_REQUIRE_MESSAGE(minted, util::ErrorString(minted).original);
        BOOST_CHECK_EQUAL(minted->height_pad, pad);
        BOOST_CHECK_EQUAL(minted->current_height, current_height);
        BOOST_CHECK_EQUAL(minted->target_height, target_height);
        BOOST_CHECK_EQUAL(minted->destroy_cost_height, target_height);
        BOOST_CHECK_EQUAL(minted->valid_until_height, target_height);
        BOOST_CHECK(minted->slack_res > 0 || minted->exact_res_out == minted->res_out);
        mint_tx = minted->txr.tx;
    }

    wallet->CommitTransaction(mint_tx, {}, {});
    if (!m_node.mempool->exists(GenTxid::Txid(mint_tx->GetHash()))) {
        LOCK(cs_main);
        const MempoolAcceptResult accepted = m_node.chainman->ProcessTransaction(mint_tx);
        BOOST_REQUIRE_EQUAL(accepted.m_result_type, MempoolAcceptResult::ResultType::VALID);
    }
    BOOST_REQUIRE(m_node.mempool->exists(GenTxid::Txid(mint_tx->GetHash())));

    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    BOOST_CHECK_MESSAGE(m_node.mempool->exists(GenTxid::Txid(mint_tx->GetHash())),
                        "padded mint must survive the first empty block");

    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    BOOST_CHECK_MESSAGE(!m_node.mempool->exists(GenTxid::Txid(mint_tx->GetHash())),
                        "padded mint must be evicted once spendheight exceeds target_height");

    {
        LOCK(wallet->cs_wallet);
        wallet->SetLastBlockProcessed(
            WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain().Height()),
            WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain().Tip()->GetBlockHash()));
        wallet->transactionRemovedFromMempool(mint_tx, MemPoolRemovalReason::REORG);
        wallet->AbandonHeightInvalidMints();
        const CWalletTx* wtx = wallet->GetWalletTx(mint_tx->GetHash());
        BOOST_REQUIRE(wtx);
        BOOST_CHECK(wtx->isAbandoned());
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace wallet
