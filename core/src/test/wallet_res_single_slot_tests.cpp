// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>
#include <outputtype.h>
#include <script/descriptor.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <sync.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <thread>
#include <vector>

using wallet::CWallet;
using wallet::CreateMockWalletDatabase;
using wallet::DatabaseOptions;
using wallet::DBErrors;
using wallet::DescriptorScriptPubKeyMan;
using wallet::DuplicateMockDatabase;
using wallet::WalletDescriptor;

namespace {

void InitNativeDualDescriptorWallet(TestingSetup& setup, CWallet& w)
{
    LOCK2(w.cs_wallet, Assert(setup.m_node.chainman)->GetMutex());
    w.SetWalletFlag(wallet::WALLET_FLAG_DESCRIPTORS);
    w.SetLastBlockProcessed(setup.m_node.chainman->ActiveHeight(), setup.m_node.chainman->ActiveTip()->GetBlockHash());
    w.SetupDescriptorScriptPubKeyMans();
}

bool WitnessVersion(const CTxDestination& d, int& version)
{
    const CScript script = GetScriptForDestination(d);
    std::vector<unsigned char> prog;
    return script.IsWitnessProgram(version, prog);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(wallet_res_single_slot_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(res_getnewaddress_repeats_same_destination)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitNativeDualDescriptorWallet(*this, w);
    LOCK(w.cs_wallet);
    const auto a1 = w.GetNewDestination(OutputType::BECH32M, "");
    const auto a2 = w.GetNewDestination(OutputType::BECH32M, "");
    BOOST_REQUIRE(a1);
    BOOST_REQUIRE(a2);
    BOOST_CHECK(EncodeDestination(*a1) == EncodeDestination(*a2));
    int v1 = -1, v2 = -1;
    BOOST_CHECK(WitnessVersion(*a1, v1));
    BOOST_CHECK(WitnessVersion(*a2, v2));
    BOOST_CHECK_EQUAL(v1, 1);
    BOOST_CHECK_EQUAL(v2, 1);
}

BOOST_AUTO_TEST_CASE(oru_res_destinations_do_not_cross_witness_domains)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitNativeDualDescriptorWallet(*this, w);
    LOCK(w.cs_wallet);
    const auto oru = w.GetNewDestination(OutputType::BECH32, "");
    const auto res = w.GetNewDestination(OutputType::BECH32M, "");
    BOOST_REQUIRE(oru);
    BOOST_REQUIRE(res);
    int voru = -1, vres = -1;
    BOOST_CHECK(WitnessVersion(*oru, voru));
    BOOST_CHECK(WitnessVersion(*res, vres));
    BOOST_CHECK_EQUAL(voru, 0);
    BOOST_CHECK_EQUAL(vres, 1);
    BOOST_CHECK(EncodeDestination(*oru) != EncodeDestination(*res));
}

BOOST_AUTO_TEST_CASE(res_internal_change_differs_from_external_receive)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitNativeDualDescriptorWallet(*this, w);
    LOCK(w.cs_wallet);
    const auto recv = w.GetNewDestination(OutputType::BECH32M, "");
    const auto change = w.GetNewChangeDestination(OutputType::BECH32M);
    BOOST_REQUIRE(recv);
    BOOST_REQUIRE(change);
    BOOST_CHECK(EncodeDestination(*recv) != EncodeDestination(*change));
}

BOOST_AUTO_TEST_CASE(res_descriptor_next_index_stays_zero_after_repeated_mint)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitNativeDualDescriptorWallet(*this, w);
    LOCK(w.cs_wallet);
    auto* spkm = dynamic_cast<DescriptorScriptPubKeyMan*>(w.GetScriptPubKeyMan(OutputType::BECH32M, false));
    BOOST_REQUIRE(spkm);
    BOOST_REQUIRE(w.GetNewDestination(OutputType::BECH32M, ""));
    BOOST_REQUIRE(w.GetNewDestination(OutputType::BECH32M, ""));
    LOCK(spkm->cs_desc_man);
    BOOST_CHECK_EQUAL(spkm->GetWalletDescriptor().next_index, 0);
}

BOOST_AUTO_TEST_CASE(import_active_ard_descriptor_via_wallet_descriptor_api)
{
    CWallet w_src(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitNativeDualDescriptorWallet(*this, w_src);
    std::string desc_str;
    {
        LOCK(w_src.cs_wallet);
        auto* spkm = dynamic_cast<DescriptorScriptPubKeyMan*>(w_src.GetScriptPubKeyMan(OutputType::BECH32M, false));
        BOOST_REQUIRE(spkm);
        BOOST_REQUIRE(spkm->GetDescriptorString(desc_str, true));
    }

    CWallet w_dst(m_node.chain.get(), "", CreateMockWalletDatabase());
    BOOST_CHECK_EQUAL(w_dst.LoadWallet(), DBErrors::LOAD_OK);
    FlatSigningProvider keys;
    std::string error;
    std::unique_ptr<Descriptor> parsed = Parse(desc_str, keys, error, /*require_checksum=*/true);
    BOOST_REQUIRE_MESSAGE(parsed, error);
    BOOST_CHECK(!parsed->IsRange());
    BOOST_REQUIRE(parsed->GetOutputType());
    BOOST_CHECK(*parsed->GetOutputType() == OutputType::BECH32M);

    WalletDescriptor w_desc(std::move(parsed), /*creation_time=*/1, /*range_start=*/0, /*range_end=*/1, /*next_index=*/0);
    {
        LOCK(w_dst.cs_wallet);
        w_dst.SetWalletFlag(wallet::WALLET_FLAG_DESCRIPTORS);
        w_dst.SetLastBlockProcessed(m_node.chainman->ActiveHeight(), m_node.chainman->ActiveTip()->GetBlockHash());
        auto* spk_man = w_dst.AddWalletDescriptor(w_desc, keys, "", /*internal=*/false);
        BOOST_REQUIRE(spk_man != nullptr);
        w_dst.AddActiveScriptPubKeyMan(spk_man->GetID(), OutputType::BECH32M, /*internal=*/false);
    }
    const auto dest = w_dst.GetNewDestination(OutputType::BECH32M, "");
    BOOST_REQUIRE(dest);
    int ver = -1;
    BOOST_CHECK(WitnessVersion(*dest, ver));
    BOOST_CHECK_EQUAL(ver, 1);
}

BOOST_AUTO_TEST_CASE(duplicate_mock_database_preserves_res_descriptor_state)
{
    CWallet w1(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitNativeDualDescriptorWallet(*this, w1);
    const CTxDestination res_before = *Assert(w1.GetNewDestination(OutputType::BECH32M, ""));
    w1.Flush();

    DatabaseOptions dup_opts{};
    auto db2 = DuplicateMockDatabase(w1.GetDatabase(), dup_opts);
    CWallet w2(m_node.chain.get(), "", std::move(db2));
    BOOST_CHECK_EQUAL(w2.LoadWallet(), DBErrors::LOAD_OK);

    LOCK2(w1.cs_wallet, w2.cs_wallet);
    const auto res_after = w2.GetNewDestination(OutputType::BECH32M, "");
    BOOST_REQUIRE(res_after);
    BOOST_CHECK(EncodeDestination(*res_after) == EncodeDestination(res_before));
}

BOOST_AUTO_TEST_CASE(concurrent_res_getnewdestination_under_cs_wallet)
{
    CWallet w(m_node.chain.get(), "", CreateMockWalletDatabase());
    InitNativeDualDescriptorWallet(*this, w);
    CTxDestination first;
    {
        LOCK(w.cs_wallet);
        first = *Assert(w.GetNewDestination(OutputType::BECH32M, ""));
    }
    std::atomic<int> failures{0};
    auto worker = [&w, &first, &failures] {
        for (int i = 0; i < 50; ++i) {
            LOCK(w.cs_wallet);
            const auto d = w.GetNewDestination(OutputType::BECH32M, "");
            if (!d || EncodeDestination(*d) != EncodeDestination(first)) {
                ++failures;
            }
        }
    };
    std::vector<std::thread> threads;
    threads.reserve(4);
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back(worker);
    }
    for (auto& th : threads) {
        th.join();
    }
    BOOST_CHECK_EQUAL(failures.load(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
