// Copyright (c) 2026 The RIGOR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>
#include <outputtype.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <wallet/exportprivkey.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

using wallet::AddressPrivKeyExport;
using wallet::CWallet;
using wallet::CreateMockWalletDatabase;
using wallet::ExportAddressPrivateKey;
using wallet::WALLET_FLAG_DESCRIPTORS;

namespace {

void InitDualAssetDescriptorWallet(TestingSetup& setup, CWallet& w)
{
    LOCK2(w.cs_wallet, Assert(setup.m_node.chainman)->GetMutex());
    w.SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
    w.SetLastBlockProcessed(setup.m_node.chainman->ActiveHeight(), setup.m_node.chainman->ActiveTip()->GetBlockHash());
    w.SetupDescriptorScriptPubKeyMans();
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(wallet_key_model_status_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(default_wallet_reports_oru_and_res_hd)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDualAssetDescriptorWallet(*this, w);

    BOOST_CHECK(w.IsHDEnabled());
    BOOST_CHECK(w.IsOruHDEnabled());
    BOOST_CHECK(w.IsResHDEnabled());
    BOOST_CHECK(!w.IsResDedicatedKeyMode());
}

BOOST_AUTO_TEST_CASE(oru_hd_allows_multiple_receive_addresses)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDualAssetDescriptorWallet(*this, w);
    BOOST_REQUIRE(w.IsOruHDEnabled());

    LOCK(w.cs_wallet);
    const auto a1 = w.GetNewDestination(OutputType::BECH32, "");
    const auto a2 = w.GetNewDestination(OutputType::BECH32, "");
    BOOST_REQUIRE(a1);
    BOOST_REQUIRE(a2);
    BOOST_CHECK(EncodeDestination(*a1) != EncodeDestination(*a2));
}

BOOST_AUTO_TEST_CASE(res_hd_allows_multiple_receive_addresses)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDualAssetDescriptorWallet(*this, w);
    BOOST_REQUIRE(w.IsResHDEnabled());
    BOOST_REQUIRE(!w.IsResDedicatedKeyMode());

    LOCK(w.cs_wallet);
    const auto a1 = w.GetNewDestination(OutputType::BECH32M, "");
    const auto a2 = w.GetNewDestination(OutputType::BECH32M, "");
    BOOST_REQUIRE(a1);
    BOOST_REQUIRE(a2);
    BOOST_CHECK(EncodeDestination(*a1) != EncodeDestination(*a2));
}

BOOST_AUTO_TEST_CASE(key_model_status_does_not_block_private_key_export)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDualAssetDescriptorWallet(*this, w);
    BOOST_REQUIRE(w.IsOruHDEnabled());
    BOOST_REQUIRE(w.IsResHDEnabled());
    BOOST_REQUIRE(w.IsHDEnabled());

    CTxDestination oru_dest;
    CTxDestination res_dest;
    {
        LOCK(w.cs_wallet);
        const auto oru = w.GetNewDestination(OutputType::BECH32, "");
        const auto res = w.GetNewDestination(OutputType::BECH32M, "");
        BOOST_REQUIRE(oru);
        BOOST_REQUIRE(res);
        oru_dest = *oru;
        res_dest = *res;
    }

    const AddressPrivKeyExport oru_export = ExportAddressPrivateKey(w, EncodeDestination(oru_dest), ASSET_ORU);
    BOOST_CHECK_EQUAL(oru_export.asset, "ORU");
    BOOST_CHECK_EQUAL(oru_export.format, "WIF");
    BOOST_CHECK(!oru_export.private_key.empty());

    const AddressPrivKeyExport res_export = ExportAddressPrivateKey(w, EncodeDestination(res_dest), ASSET_RES);
    BOOST_CHECK_EQUAL(res_export.asset, "RES");
    BOOST_CHECK_EQUAL(res_export.format, "ed25519");
    BOOST_CHECK_EQUAL(res_export.private_key.size(), 128u);
}

BOOST_AUTO_TEST_SUITE_END()
