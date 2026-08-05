// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <wallet/receive.h>
#include <wallet/reuse_domain.h>
#include <wallet/test/util.h>

#include <boost/test/unit_test.hpp>

#include <array>

BOOST_FIXTURE_TEST_SUITE(wallet_reuse_domain_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(oru_reuse_semantics_apply_by_asset)
{
    BOOST_CHECK(wallet::OruReuseSemanticsApply(ASSET_ORU));
    BOOST_CHECK(!wallet::OruReuseSemanticsApply(ASSET_RES));
}

BOOST_AUTO_TEST_CASE(oru_reuse_semantics_apply_by_script)
{
    CKey key;
    key.MakeNewKey(true);
    const CScript oru_spk = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey()));
    std::array<unsigned char, 32> res_prog_bytes{};
    res_prog_bytes.fill(0xcd);
    const CScript res_spk = GetScriptForDestination(WitnessV1Res{XOnlyPubKey{res_prog_bytes}});

    BOOST_CHECK(wallet::OruReuseSemanticsApply(oru_spk));
    BOOST_CHECK(!wallet::OruReuseSemanticsApply(res_spk));
}

BOOST_AUTO_TEST_CASE(effective_avoid_reuse_for_asset)
{
    BOOST_CHECK(wallet::EffectiveAvoidReuseForAsset(true, ASSET_ORU));
    BOOST_CHECK(!wallet::EffectiveAvoidReuseForAsset(true, ASSET_RES));
    BOOST_CHECK(!wallet::EffectiveAvoidReuseForAsset(false, ASSET_ORU));
}

BOOST_AUTO_TEST_SUITE_END()

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(wallet_reuse_domain_chain_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(res_balance_ignores_avoid_reuse)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);

    const AssetBalanceDetail res_clean = GetAssetBalance(*wallet, ASSET_RES, 0, /*avoid_reuse=*/true);
    const AssetBalanceDetail res_all = GetAssetBalance(*wallet, ASSET_RES, 0, /*avoid_reuse=*/false);
    BOOST_CHECK_EQUAL(res_clean.trusted, res_all.trusted);
    BOOST_CHECK_EQUAL(res_clean.pending, res_all.pending);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet
