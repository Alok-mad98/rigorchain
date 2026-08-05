// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <univalue.h>
#include <wallet/receive.h>
#include <wallet/rpc/amount_domain.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(wallet_rpc_amount_domain_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(push_received_tally_splits_native_domains)
{
    UniValue obj(UniValue::VOBJ);
    wallet::PushReceivedTallyAmountFields(obj, 2 * COIN, 3 * RES_COIN);
    BOOST_CHECK(obj.exists("amount_oru"));
    BOOST_CHECK(obj.exists("amount_res"));
    BOOST_CHECK(obj.exists("amount"));
    BOOST_CHECK_EQUAL(obj["amount_oru"].getValStr(), obj["amount"].getValStr());
}

BOOST_AUTO_TEST_CASE(push_wallet_tx_domain_amount_fields)
{
    wallet::WalletTxDomainNet net;
    net.oru = -5000;
    net.res = 2 * RES_COIN;
    UniValue entry(UniValue::VOBJ);
    wallet::PushWalletTxDomainAmountFields(entry, net);
    BOOST_CHECK(entry.exists("amount_oru"));
    BOOST_CHECK(entry.exists("amount_res"));
    BOOST_CHECK(entry.exists("amount"));
}

BOOST_AUTO_TEST_CASE(push_send_category_oru_fee_fields)
{
    UniValue entry(UniValue::VOBJ);
    wallet::PushSendCategoryOruFeeFields(entry, 2500);
    BOOST_REQUIRE(entry.exists("fee"));
    BOOST_REQUIRE(entry.exists("fee_oru"));
    BOOST_CHECK_EQUAL(entry["fee"].getValStr(), "-0.00002500");
    BOOST_CHECK_EQUAL(entry["fee_oru"].getValStr(), "0.00002500");
}

BOOST_AUTO_TEST_SUITE_END()

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(wallet_rpc_amount_domain_chain_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(credit_debit_for_asset_do_not_mix_domains)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);

    CAmount oru_credit{0};
    CAmount res_credit{0};
    {
        LOCK(wallet->cs_wallet);
        for (const auto& [hash, wtx] : wallet->mapWallet) {
            oru_credit += CachedTxGetCreditForAsset(*wallet, wtx, ASSET_ORU, ISMINE_ALL);
            res_credit += CachedTxGetCreditForAsset(*wallet, wtx, ASSET_RES, ISMINE_ALL);
        }
    }
    BOOST_CHECK(oru_credit > 0);
    BOOST_CHECK_EQUAL(res_credit, 0);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet
