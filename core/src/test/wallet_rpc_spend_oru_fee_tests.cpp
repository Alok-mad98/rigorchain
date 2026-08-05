// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <key.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <wallet/fee_domain.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <set>

BOOST_FIXTURE_TEST_SUITE(wallet_rpc_spend_oru_fee_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(push_rpc_oru_fee_fields)
{
    UniValue obj(UniValue::VOBJ);
    wallet::PushRpcOruNetworkFeeFields(obj, "fee", 5000);
    BOOST_CHECK_EQUAL(obj["fee"].getValStr(), "0.00005000");
    BOOST_CHECK_EQUAL(obj["fee_oru"].getValStr(), "0.00005000");
    BOOST_CHECK_EQUAL(obj["fee_asset"].get_str(), "ORU");
}

BOOST_AUTO_TEST_CASE(reject_subtract_fee_from_res_output)
{
    CMutableTransaction tx;
    std::array<unsigned char, 32> res_prog_bytes{};
    res_prog_bytes.fill(0xab);
    tx.vout.emplace_back(1 * RES_COIN, GetScriptForDestination(WitnessV1Res{XOnlyPubKey{res_prog_bytes}}));
    const std::set<int> subtract{0};
    const auto err = wallet::CheckSubtractFeeFromResOutputs(tx, subtract);
    BOOST_REQUIRE(err.has_value());
    BOOST_CHECK(err->original.find("ORU network fee") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(res_transfer_rejects_subtract_fee_recipient)
{
    const std::vector<wallet::CRecipient> vec{
        {CScript() << OP_RETURN, 1 * RES_COIN, /*subtract=*/true, ASSET_RES}};
    const auto err = wallet::CheckResTransferFeeSemantics(vec);
    BOOST_REQUIRE(err.has_value());
}

BOOST_AUTO_TEST_SUITE_END()
