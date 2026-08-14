// Copyright (c) 2026 The RIGOR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>
#include <outputtype.h>
#include <rpc/request.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>
#include <wallet/exportprivkey.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>
#include <string>

using wallet::AddressPrivKeyExport;
using wallet::CWallet;
using wallet::CreateMockWalletDatabase;
using wallet::ExportAddressPrivateKey;
using wallet::WALLET_FLAG_DESCRIPTORS;
using wallet::WALLET_FLAG_DISABLE_PRIVATE_KEYS;

namespace {

void InitDescriptorWallet(TestingSetup& setup, CWallet& w)
{
    LOCK2(w.cs_wallet, Assert(setup.m_node.chainman)->GetMutex());
    w.SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
    w.SetLastBlockProcessed(setup.m_node.chainman->ActiveHeight(), setup.m_node.chainman->ActiveTip()->GetBlockHash());
    w.SetupDescriptorScriptPubKeyMans();
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(wallet_privkey_export_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(oru_export_wif_roundtrip_address)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWallet(*this, w);

    CTxDestination dest;
    {
        LOCK(w.cs_wallet);
        const auto oru = w.GetNewDestination(OutputType::BECH32, "");
        BOOST_REQUIRE(oru);
        dest = *oru;
    }

    const std::string addr = EncodeDestination(dest);
    const AddressPrivKeyExport exported = ExportAddressPrivateKey(w, addr, ASSET_ORU);
    BOOST_CHECK_EQUAL(exported.asset, "ORU");
    BOOST_CHECK_EQUAL(exported.format, "WIF");
    BOOST_CHECK_EQUAL(exported.address, addr);
    BOOST_CHECK(!exported.private_key.empty());

    const CKey key = DecodeSecret(exported.private_key);
    BOOST_REQUIRE(key.IsValid());
    BOOST_CHECK_EQUAL(EncodeDestination(WitnessV0KeyHash(key.GetPubKey().GetID())), addr);
}

BOOST_AUTO_TEST_CASE(res_export_ed25519_secret_format)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWallet(*this, w);

    CTxDestination dest;
    {
        LOCK(w.cs_wallet);
        const auto res = w.GetNewDestination(OutputType::BECH32M, "");
        BOOST_REQUIRE(res);
        dest = *res;
    }

    const std::string addr = EncodeDestination(dest);
    const AddressPrivKeyExport exported = ExportAddressPrivateKey(w, addr, ASSET_RES);
    BOOST_CHECK_EQUAL(exported.asset, "RES");
    BOOST_CHECK_EQUAL(exported.format, "ed25519");
    BOOST_CHECK_EQUAL(exported.address, addr);
    BOOST_CHECK_EQUAL(exported.private_key.size(), 128u);
}

BOOST_AUTO_TEST_CASE(export_rejects_wrong_asset_and_unknown)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWallet(*this, w);

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

    BOOST_CHECK_EXCEPTION(
        ExportAddressPrivateKey(w, EncodeDestination(res_dest), ASSET_ORU),
        UniValue,
        [](const UniValue& e) { return e["message"].get_str().find("Only ORU") != std::string::npos; });

    BOOST_CHECK_EXCEPTION(
        ExportAddressPrivateKey(w, EncodeDestination(oru_dest), ASSET_RES),
        UniValue,
        [](const UniValue& e) { return e["message"].get_str().find("Only RES") != std::string::npos; });

    CWallet other(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWallet(*this, other);
    CTxDestination foreign;
    {
        LOCK(other.cs_wallet);
        const auto addr = other.GetNewDestination(OutputType::BECH32, "");
        BOOST_REQUIRE(addr);
        foreign = *addr;
    }

    BOOST_CHECK_EXCEPTION(
        ExportAddressPrivateKey(w, EncodeDestination(foreign), ASSET_ORU),
        UniValue,
        [](const UniValue& e) { return e["message"].get_str() == "Address not found in wallet"; });
}

BOOST_AUTO_TEST_CASE(export_rejects_watch_only_flag)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    {
        LOCK2(w.cs_wallet, Assert(m_node.chainman)->GetMutex());
        w.SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
        w.SetWalletFlag(WALLET_FLAG_DISABLE_PRIVATE_KEYS);
        w.SetLastBlockProcessed(m_node.chainman->ActiveHeight(), m_node.chainman->ActiveTip()->GetBlockHash());
    }

    // Watch-only wallets cannot generate spendable destinations via SetupDescriptorScriptPubKeyMans
    // the same way; validate the flag gate with a syntactically valid foreign ORU destination.
    CWallet donor(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWallet(*this, donor);
    CTxDestination dest;
    {
        LOCK(donor.cs_wallet);
        const auto oru = donor.GetNewDestination(OutputType::BECH32, "");
        BOOST_REQUIRE(oru);
        dest = *oru;
    }

    BOOST_CHECK_EXCEPTION(
        ExportAddressPrivateKey(w, EncodeDestination(dest), ASSET_ORU),
        UniValue,
        [](const UniValue& e) {
            return e["message"].get_str() == "Private key unavailable for watch-only descriptor";
        });
}

BOOST_AUTO_TEST_SUITE_END()
