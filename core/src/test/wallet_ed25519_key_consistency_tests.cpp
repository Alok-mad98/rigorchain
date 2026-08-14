// Copyright (c) 2026 The RIGOR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/ed25519_provider.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <key.h>
#include <key_io.h>
#include <outputtype.h>
#include <pubkey.h>
#include <script/standard.h>
#include <span.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <validation.h>
#include <wallet/exportprivkey.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <algorithm>
#include <array>
#include <boost/test/unit_test.hpp>
#include <cstring>
#include <string>
#include <vector>

using wallet::AddressPrivKeyExport;
using wallet::CWallet;
using wallet::CreateMockWalletDatabase;
using wallet::ExportAddressPrivateKey;
using wallet::WALLET_FLAG_DESCRIPTORS;

namespace {

void InitDescriptorWallet(TestingSetup& setup, CWallet& w)
{
    LOCK2(w.cs_wallet, Assert(setup.m_node.chainman)->GetMutex());
    w.SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
    w.SetLastBlockProcessed(setup.m_node.chainman->ActiveHeight(), setup.m_node.chainman->ActiveTip()->GetBlockHash());
    w.SetupDescriptorScriptPubKeyMans();
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(wallet_ed25519_key_consistency_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(oru_export_wif_is_ed25519_signer)
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

    const AddressPrivKeyExport exported = ExportAddressPrivateKey(w, EncodeDestination(dest), ASSET_ORU);
    BOOST_CHECK_EQUAL(exported.format, "WIF");
    BOOST_CHECK_EQUAL(exported.asset, "ORU");

    const CKey key = DecodeSecret(exported.private_key);
    BOOST_REQUIRE(key.IsValid());
    const CPubKey pubkey = key.GetPubKey();
    BOOST_REQUIRE_EQUAL(pubkey.size(), CPubKey::COMPRESSED_SIZE);
    BOOST_CHECK_EQUAL(pubkey[0], 0x00);
    BOOST_CHECK_EQUAL(EncodeDestination(WitnessV0KeyHash(pubkey.GetID())), exported.address);

    uint256 hash;
    static const unsigned char oru_msg[] = "rigor-oru-key-consistency";
    CSHA256().Write(oru_msg, sizeof(oru_msg) - 1).Finalize(hash.begin());
    std::vector<unsigned char> sig;
    BOOST_REQUIRE(key.Sign(hash, sig));
    BOOST_REQUIRE_EQUAL(sig.size(), ed25519::SIGNATURE_SIZE);
    BOOST_CHECK(pubkey.Verify(hash, sig));

    std::vector<unsigned char> bad = sig;
    bad[0] ^= 0x01;
    BOOST_CHECK(!pubkey.Verify(hash, bad));
}

BOOST_AUTO_TEST_CASE(res_export_hex_is_ed25519_seed_and_secret)
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

    const AddressPrivKeyExport exported = ExportAddressPrivateKey(w, EncodeDestination(dest), ASSET_RES);
    BOOST_CHECK_EQUAL(exported.format, "ed25519");
    BOOST_CHECK_EQUAL(exported.asset, "RES");
    BOOST_REQUIRE_EQUAL(exported.private_key.size(), 128u);

    const std::vector<unsigned char> secret_bytes = ParseHex(exported.private_key);
    BOOST_REQUIRE_EQUAL(secret_bytes.size(), ed25519::SECRETKEY_SIZE);
    std::array<unsigned char, ed25519::PUBKEY_SIZE> pk{};
    std::array<unsigned char, ed25519::SECRETKEY_SIZE> sk{};
    BOOST_REQUIRE(ed25519::SeedToKeypair(
        Span<const unsigned char>{secret_bytes.data(), ed25519::SEED_SIZE}, pk, sk));
    BOOST_CHECK(std::equal(sk.begin(), sk.end(), secret_bytes.begin()));

    const CTxDestination derived{WitnessV1Res{XOnlyPubKey{Span<const unsigned char>{pk.data(), pk.size()}}}};
    BOOST_CHECK_EQUAL(EncodeDestination(derived), exported.address);

    uint256 hash;
    static const unsigned char res_msg[] = "rigor-res-key-consistency";
    CSHA256().Write(res_msg, sizeof(res_msg) - 1).Finalize(hash.begin());
    std::vector<unsigned char> sig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{hash.begin(), 32}, sk, sig));
    BOOST_CHECK(ed25519::VerifyDetached(sig, Span{hash.begin(), 32}, pk));
}

BOOST_AUTO_TEST_CASE(oru_wif_roundtrip_recreates_same_address)
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

    const AddressPrivKeyExport exported = ExportAddressPrivateKey(w, EncodeDestination(dest), ASSET_ORU);
    const CKey imported = DecodeSecret(exported.private_key);
    BOOST_REQUIRE(imported.IsValid());
    BOOST_CHECK_EQUAL(EncodeSecret(imported), exported.private_key);
    BOOST_CHECK_EQUAL(EncodeDestination(WitnessV0KeyHash(imported.GetPubKey().GetID())), exported.address);
}

BOOST_AUTO_TEST_SUITE_END()
