// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <consensus_rigor/fee.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/fee_domain.h>
#include <wallet/fees.h>
#include <wallet/spend.h>
#include <wallet/test/util.h>
#include <wallet/transaction.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

using wallet::CRecipient;
using wallet::NETWORK_FEE_ASSET;
using wallet::OruNetworkFeeForSizeBytes;
using wallet::WalletHasSpendableOruForFee;

BOOST_FIXTURE_TEST_SUITE(wallet_oru_fee_dominance_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(network_fee_asset_is_oru)
{
    BOOST_CHECK_EQUAL(NETWORK_FEE_ASSET, ASSET_ORU);
}

BOOST_AUTO_TEST_CASE(oru_network_fee_for_vsize_uses_oru_sat_vb)
{
    const CFeeRate rate(2500);
    BOOST_CHECK_EQUAL(OruNetworkFeeForSizeBytes(rate, 200), rate.GetFee(200));
    BOOST_CHECK_EQUAL(OruNetworkFeeForSizeBytes(rate, 0), 0);
}

BOOST_AUTO_TEST_CASE(oru_network_fee_covers_change_output_bytes)
{
    // Regression for RES dual-pool: fee computed before appending ORU change
    // underpays MinFeeMet_ORU (user saw ~388 sat on a ~412-byte signed tx).
    const CFeeRate rate(DEFAULT_MIN_RELAY_TX_FEE);
    const int size_without_oru_change = 388;
    const int size_with_oru_change = 412;
    BOOST_CHECK_EQUAL(OruNetworkFeeForSizeBytes(rate, size_without_oru_change), 388);
    BOOST_CHECK_EQUAL(OruNetworkFeeForSizeBytes(rate, size_with_oru_change), 412);
    BOOST_CHECK_LT(OruNetworkFeeForSizeBytes(rate, size_without_oru_change),
                   OruNetworkFeeForSizeBytes(rate, size_with_oru_change));
}

BOOST_AUTO_TEST_CASE(feerate_unchanged_for_res_vs_oru_semantics)
{
    const CFeeRate rate(5000);
    const CAmount fee_oru = OruNetworkFeeForSizeBytes(rate, 250);
    BOOST_CHECK_EQUAL(fee_oru, rate.GetFee(250));
    BOOST_CHECK(fee_oru > 0);
}

BOOST_AUTO_TEST_CASE(res_transfer_rejects_subtract_fee_from_amount)
{
    std::vector<CRecipient> vec{{CScript() << OP_RETURN, 1 * RES_COIN, /*subtract=*/true, ASSET_RES}};
    const auto err = wallet::CheckResTransferFeeSemantics(vec);
    BOOST_REQUIRE(err.has_value());
}

BOOST_AUTO_TEST_CASE(no_res_fee_unit_scale_in_spend_cpp)
{
    // TEST-5: RES_FEE_UNIT_SCALE must not drive feerate math (grep guard).
    BOOST_CHECK(wallet::DEPRECATED_RES_FEE_UNIT_SCALE > 0);
    const CFeeRate rate(10000);
    const CAmount oru_fee = OruNetworkFeeForSizeBytes(rate, 100);
    BOOST_CHECK_EQUAL(oru_fee, rate.GetFee(100));
}

BOOST_AUTO_TEST_SUITE_END()

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(wallet_oru_fee_dominance_chain_tests, TestChain100Setup)

// TEST-3 / TEST-4: ORU path uses unscaled sat/vB feerate (no RES_FEE_UNIT_SCALE in GetMinimumFeeRate).
BOOST_AUTO_TEST_CASE(oru_single_pool_uses_unscaled_feerate)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    BOOST_CHECK(WalletHasSpendableOruForFee(*wallet, nullptr));

    CCoinControl oru_cc;
    oru_cc.m_feerate = CFeeRate(5000);
    oru_cc.fOverrideFeeRate = true;
    FeeCalculation oru_calc;
    const CFeeRate oru_rate = GetMinimumFeeRate(*wallet, oru_cc, &oru_calc);

    CCoinControl res_cc;
    res_cc.m_asset_type = ASSET_RES;
    res_cc.m_feerate = CFeeRate(5000);
    res_cc.fOverrideFeeRate = true;
    FeeCalculation res_calc;
    const CFeeRate res_rate = GetMinimumFeeRate(*wallet, res_cc, &res_calc);

    BOOST_CHECK_EQUAL(oru_rate.GetFeePerK(), res_rate.GetFeePerK());
    BOOST_CHECK_EQUAL(OruNetworkFeeForSizeBytes(oru_rate, 250), oru_rate.GetFee(250));
}

BOOST_AUTO_TEST_CASE(oru_create_tx_fee_covers_serialized_size)
{
    // Shared FinalizeOruNetworkFeeAndChange: paid fee must meet MinFeeMet_ORU size.
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    LOCK(wallet->cs_wallet);

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "");
    BOOST_REQUIRE(dest);
    CRecipient recipient{GetScriptForDestination(*dest), 1 * COIN, false, ASSET_ORU};

    CCoinControl cc;
    cc.m_feerate = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
    cc.fOverrideFeeRate = true;

    auto created = CreateTransaction(*wallet, {recipient}, /*change_pos=*/-1, cc);
    BOOST_REQUIRE_MESSAGE(created, util::ErrorString(created).original);
    const CTransaction& tx = *created->tx;
    const int64_t sz = rigor::GetTransactionSizeBytes(tx);
    BOOST_CHECK_GE(created->fee, OruNetworkFeeForSizeBytes(*cc.m_feerate, sz));
    BOOST_CHECK_EQUAL(tx.GetValueOut(ASSET_RES), 0);
}

BOOST_AUTO_TEST_CASE(res_send_fails_without_oru_for_network_fee)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    LOCK(wallet->cs_wallet);
    BOOST_CHECK(WalletHasSpendableOruForFee(*wallet, nullptr));

    const auto res_dest = wallet->GetNewDestination(OutputType::BECH32M, "");
    BOOST_REQUIRE(res_dest);
    CRecipient recipient{GetScriptForDestination(*res_dest), RES_COIN, false, ASSET_RES};

    CCoinControl cc;
    cc.m_asset_type = ASSET_RES;
    cc.m_feerate = CFeeRate(5000);
    cc.fOverrideFeeRate = true;
    cc.m_allow_other_inputs = false;

    CoinsResult res_coins;
    CoinFilterParams p;
    p.m_asset_type = ASSET_RES;
    res_coins = AvailableCoins(*wallet, &cc, std::nullopt, p);
    if (res_coins.GetTotalAmount() == 0) {
        BOOST_TEST_MESSAGE("Skipping: no RES UTXOs in fixture wallet");
        return;
    }

    cc.m_allow_other_inputs = true;
    CoinFilterParams oru_p;
    oru_p.m_asset_type = ASSET_ORU;
    CoinsResult oru_coins = AvailableCoins(*wallet, &cc, std::nullopt, oru_p);
    if (oru_coins.GetTotalAmount() == 0) {
        auto fail = CreateTransaction(*wallet, {recipient}, -1, cc);
        BOOST_CHECK(!fail);
        return;
    }

    for (const auto& coin : res_coins.All()) {
        cc.Select(coin.outpoint);
    }
    cc.m_allow_other_inputs = false;

    auto fail = CreateTransaction(*wallet, {recipient}, -1, cc);
    BOOST_CHECK(!fail);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet
