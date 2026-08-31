// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <pubkey.h>
#include <script/rigor_predicate_registry.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <wallet/test/util.h>
#include <wallet/types.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

using wallet::CWallet;
using wallet::CreateMockWalletDatabase;
using wallet::ISMINE_NO;
using wallet::ISMINE_SPENDABLE;
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

BOOST_FIXTURE_TEST_SUITE(rigor_wallet_ownership_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(checksig_script_is_never_mine)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    w.SetupLegacyScriptPubKeyMan();
    CKey key;
    key.MakeNewKey(true);
    {
        LOCK(w.cs_wallet);
        BOOST_CHECK(w.GetOrCreateLegacyScriptPubKeyMan()->AddKeyPubKey(key, key.GetPubKey()));
    }
    const CScript p2pk = CScript() << ToByteVector(key.GetPubKey()) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    const CScript p2pkh = CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160
                                    << ToByteVector(key.GetPubKey().GetID()) << RIGOR_RESERVED_LEGACY_EQUALVERIFY
                                    << RIGOR_RESERVED_LEGACY_CHECKSIG;
    const CScript ms = CScript() << RIGOR_PUSH_1 << ToByteVector(key.GetPubKey()) << RIGOR_PUSH_1
                                 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK(!LookupRigorPredicate(p2pk));
    BOOST_CHECK_EQUAL(w.IsMine(p2pk), ISMINE_NO);
    BOOST_CHECK_EQUAL(w.IsMine(p2pkh), ISMINE_NO);
    BOOST_CHECK_EQUAL(w.IsMine(ms), ISMINE_NO);
}

BOOST_AUTO_TEST_CASE(p2sh_p2wsh_taproot_unknown_witness_are_never_mine)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWallet(*this, w);
    const CScript p2sh = CScript() << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11)
                                   << RIGOR_RESERVED_LEGACY_EQUAL;
    const CScript p2wsh = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x22);
    const CScript v2 = CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(32, 0x33);
    BOOST_CHECK_EQUAL(w.IsMine(p2sh), ISMINE_NO);
    BOOST_CHECK_EQUAL(w.IsMine(p2wsh), ISMINE_NO);
    BOOST_CHECK_EQUAL(w.IsMine(v2), ISMINE_NO);
}

BOOST_AUTO_TEST_CASE(oru_and_res_destinations_are_mine_via_predicate)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitDescriptorWallet(*this, w);

    CTxDestination oru_dest;
    CTxDestination res_dest;
    {
        LOCK(w.cs_wallet);
        const auto oru = w.GetNewDestination(OutputType::BECH32, "");
        BOOST_REQUIRE(oru);
        oru_dest = *oru;
        const auto res = w.GetNewDestination(OutputType::BECH32M, "");
        BOOST_REQUIRE(res);
        res_dest = *res;
    }
    const CScript oru_spk = GetScriptForDestination(oru_dest);
    const CScript res_spk = GetScriptForDestination(res_dest);
    BOOST_CHECK(LookupRigorPredicate(oru_spk) == RigorPredicateID::ORU_V0_KEYHASH);
    BOOST_CHECK(LookupRigorPredicate(res_spk) == RigorPredicateID::RES_V1_NATIVE_KEY);
    BOOST_CHECK_EQUAL(w.IsMine(oru_spk), ISMINE_SPENDABLE);
    BOOST_CHECK_EQUAL(w.IsMine(res_spk), ISMINE_SPENDABLE);
}

BOOST_AUTO_TEST_SUITE_END()
