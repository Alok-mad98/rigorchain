// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/res_amount.h>
#include <key.h>
#include <key_io.h>
#include <node/psbt.h>
#include <primitives/transaction.h>
#include <psbt.h>
#include <script/standard.h>
#include <wallet/fee_domain.h>
#include <wallet/psbt_domain.h>

#include <array>

#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(wallet_psbt_domain_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(psbt_fee_asset_is_always_oru)
{
    BOOST_CHECK(NETWORK_FEE_ASSET == ASSET_ORU);
    PartiallySignedTransaction psbt;
    CMutableTransaction mtx;
    psbt.tx = mtx;
    const node::PSBTAnalysis analysis{};
    const PsbtDomainSummary summary = BuildPsbtDomainSummary(psbt, analysis);
    BOOST_CHECK(summary.fee_asset == ASSET_ORU);
}

BOOST_AUTO_TEST_CASE(psbt_output_transfer_asset_from_script)
{
    CKey key;
    key.MakeNewKey(true);
    const CPubKey pubkey = key.GetPubKey();
    const CScript oru_spk = GetScriptForDestination(WitnessV0KeyHash(pubkey.GetID()));
    BOOST_CHECK(PsbtTransferAssetFromScript(oru_spk) == ASSET_ORU);

    std::array<unsigned char, 32> res_prog_bytes{};
    res_prog_bytes.fill(0xab);
    const CScript res_spk = GetScriptForDestination(WitnessV1Res{XOnlyPubKey{res_prog_bytes}});
    BOOST_CHECK(PsbtTransferAssetFromScript(res_spk) == ASSET_RES);
}

BOOST_AUTO_TEST_CASE(psbt_unified_input_domain_oru_only)
{
    PartiallySignedTransaction psbt;
    CMutableTransaction mtx;
    CKey key;
    key.MakeNewKey(true);
    const CPubKey pubkey = key.GetPubKey();
    const CScript spk = GetScriptForDestination(WitnessV0KeyHash(pubkey.GetID()));
    mtx.vout.emplace_back(COIN, spk);
    psbt.tx = mtx;

    CMutableTransaction prev_mtx;
    prev_mtx.vout.emplace_back(COIN, spk);
    const CTransactionRef prev = MakeTransactionRef(prev_mtx);
    psbt.inputs.emplace_back();
    psbt.inputs[0].witness_utxo = CTxOut(COIN, spk);
    mtx.vin.emplace_back(COutPoint(prev->GetHash(), 0), CScript{});
    psbt.tx = mtx;

    const std::optional<AssetType> domain = PsbtUnifiedInputDomain(psbt);
    BOOST_REQUIRE(domain.has_value());
    BOOST_CHECK(*domain == ASSET_ORU);
    BOOST_CHECK(!PsbtHasMixedInputDomains(psbt));
}

BOOST_AUTO_TEST_CASE(psbt_mixed_input_domains_blocks_unsafe_fee_display)
{
    PartiallySignedTransaction psbt;
    CMutableTransaction mtx;
    CKey key;
    key.MakeNewKey(true);
    const CPubKey pubkey = key.GetPubKey();
    const CScript oru_spk = GetScriptForDestination(WitnessV0KeyHash(pubkey.GetID()));
    std::array<unsigned char, 32> res_prog_bytes{};
    res_prog_bytes.fill(0xcd);
    const CScript res_spk = GetScriptForDestination(WitnessV1Res{XOnlyPubKey{res_prog_bytes}});

    CMutableTransaction prev_oru_mtx;
    prev_oru_mtx.vout.emplace_back(COIN, oru_spk);
    const CTransactionRef prev_oru = MakeTransactionRef(prev_oru_mtx);

    CMutableTransaction prev_res_mtx;
    prev_res_mtx.vout.emplace_back(RES_COIN, res_spk);
    const CTransactionRef prev_res = MakeTransactionRef(prev_res_mtx);

    mtx.vin.emplace_back(COutPoint(prev_oru->GetHash(), 0), CScript{});
    mtx.vin.emplace_back(COutPoint(prev_res->GetHash(), 0), CScript{});
    mtx.vout.emplace_back(RES_COIN, res_spk);
    psbt.tx = mtx;

    psbt.inputs.resize(2);
    psbt.inputs[0].witness_utxo = CTxOut(COIN, oru_spk);
    psbt.inputs[1].witness_utxo = CTxOut(RES_COIN, res_spk);

    BOOST_CHECK(PsbtHasMixedInputDomains(psbt));
    BOOST_CHECK(!PsbtUnifiedInputDomain(psbt).has_value());

    node::PSBTAnalysis analysis;
    analysis.fee = 1000;
    BOOST_CHECK(!PsbtOruNetworkFeeForDisplay(psbt, analysis).has_value());
}

BOOST_AUTO_TEST_CASE(psbt_v1_destination_summary_not_taproot)
{
    std::array<unsigned char, 32> res_prog_bytes{};
    res_prog_bytes.fill(0xcd);
    const CScript res_spk = GetScriptForDestination(WitnessV1Res{XOnlyPubKey{res_prog_bytes}});
    const std::string summary = PsbtOutputDestinationSummary(res_spk);
    BOOST_CHECK(summary.find("RES") != std::string::npos);
    BOOST_CHECK(summary.find("Taproot") == std::string::npos);
    BOOST_CHECK(summary.find("script-path") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet
