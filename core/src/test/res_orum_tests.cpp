// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <consensus/convergenceres.h>
#include <consensus/res_mint_witness.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/tx_check.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/setup_common.h>

#include <cstring>

#include <boost/test/unit_test.hpp>

namespace {

/** ORU: witness v0-style 20-byte program (not v1 RES). */
const CScript g_origo_spk = CScript() << OP_0 << std::vector<unsigned char>(20, 0x11);

/** RES2: witness v1 + 32-byte program with RES2 wire prefix (ClassifyRigorTxout / CheckTxOutAssetWitnessBinding). */
CScript MakeRes2TestSpk()
{
    std::vector<unsigned char> wp(32, 0xab);
    std::memcpy(wp.data(), RigorTags::Res2WirePrefix.data(), RigorTags::Res2WirePrefix.size());
    return CScript() << OP_1 << wp;
}

const CScript g_res_spk = MakeRes2TestSpk();

bool CheckTxInputsOk(const CTransaction& tx, CCoinsViewCache& coins, CAmount& fee_out)
{
    TxValidationState state;
    fee_out = 0;
    return Consensus::CheckTxInputs(tx, state, coins, /*nSpendHeight=*/200, Params().GetConsensus(), fee_out);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(res_orum_tests, BasicTestingSetup)

// RES conservation: 10×1 RES_COIN in, 10×1 RES_COIN out. Dual-pool consensus still needs an ORU leg (zero fee: oru_in == oru_out).
BOOST_AUTO_TEST_CASE(res_conservation_ten_in_ten_out)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const uint256 h_oru{uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut(COIN, g_origo_spk), 100, false), false);

    for (uint8_t n = 1; n <= 10; ++n) {
        CTxOut prev(RES_COIN, g_res_spk);
        coins.AddCoin(COutPoint(uint256{n}, 0), Coin(CTxOut{prev}, /*nHeight=*/100, false), false);
    }

    CMutableTransaction mtx;
    mtx.vin.resize(11);
    mtx.vout.resize(11);
    mtx.vin[0].prevout = COutPoint(h_oru, 0);
    mtx.vout[0].nValue = COIN;
    mtx.vout[0].scriptPubKey = g_origo_spk;
    for (unsigned i = 0; i < 10; ++i) {
        mtx.vin[i + 1].prevout = COutPoint(uint256{static_cast<uint8_t>(i + 1)}, 0);
        mtx.vout[i + 1].nValue = RES_COIN;
        mtx.vout[i + 1].scriptPubKey = g_res_spk;
    }

    const CTransaction tx(mtx);
    TxValidationState tx_state;
    BOOST_CHECK(CheckTransaction(tx, tx_state));
    BOOST_CHECK(tx_state.IsValid());

    CAmount fee{};
    BOOST_CHECK(CheckTxInputsOk(tx, coins, fee));
    BOOST_CHECK_EQUAL(fee, 0);
}

// ORU-domain convergence: CheckTxInputs requires exactly one convergence output; ORU fee = inputs − outputs (may be 0)
BOOST_AUTO_TEST_CASE(oru_convergence_fee_matches_inputs_minus_outputs)
{
    const CAmount burn = COIN;

    CCoinsView base;
    CCoinsViewCache coins(&base);

    CTxOut prev_origo(20 * COIN, g_origo_spk);
    const uint256 h_origo{uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")};
    coins.AddCoin(COutPoint(h_origo, 0), Coin(CTxOut{prev_origo}, 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(h_origo, 0);
    mtx.vout.resize(2);
    mtx.vout[0].nValue = burn;
    mtx.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
    mtx.vout[1].nValue = 20 * COIN - burn;
    mtx.vout[1].scriptPubKey = g_origo_spk;

    const CTransaction tx(mtx);
    TxValidationState tx_state;
    BOOST_CHECK(CheckTransaction(tx, tx_state));

    CAmount fee{};
    BOOST_CHECK(CheckTxInputsOk(tx, coins, fee));
    BOOST_CHECK_EQUAL(fee, 0);
}

// Dual-pool: RES transfer with the network fee paid by the ORU leg (fee = oru_in − oru_out, not the RES delta)
BOOST_AUTO_TEST_CASE(res_transfer_network_fee_paid_in_oru)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const uint256 h_oru{uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut(COIN, g_origo_spk), 100, false), false);

    CTxOut prev_res(3 * RES_COIN, g_res_spk);
    const uint256 h_res{uint256S("0x2222222222222222222222222222222222222222222222222222222222222222")};
    coins.AddCoin(COutPoint(h_res, 0), Coin(CTxOut{prev_res}, 100, false), false);

    const CAmount res_send = 2 * RES_COIN + 50 * RES_ATOMIC_UNIT;
    const CAmount res_change = RES_COIN - 50 * RES_ATOMIC_UNIT;
    const CAmount oru_fee = 5000;

    CMutableTransaction mtx;
    mtx.vin.emplace_back(COutPoint(h_res, 0));
    mtx.vin.emplace_back(COutPoint(h_oru, 0));
    mtx.vout.emplace_back(res_send, g_res_spk);
    mtx.vout.emplace_back(res_change, g_res_spk);
    mtx.vout.emplace_back(COIN - oru_fee, g_origo_spk);

    const CTransaction tx(mtx);
    TxValidationState tx_state;
    BOOST_CHECK(CheckTransaction(tx, tx_state));
    BOOST_CHECK(tx_state.IsValid());

    CAmount fee{};
    BOOST_CHECK(CheckTxInputsOk(tx, coins, fee));
    BOOST_CHECK_EQUAL(fee, oru_fee);
}

BOOST_AUTO_TEST_CASE(res_spend_cannot_create_additional_res)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const uint256 h_oru{uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut(COIN, g_origo_spk), 100, false), false);

    CTxOut prev_res(RES_COIN, g_res_spk);
    const uint256 h_res{uint256S("0x3333333333333333333333333333333333333333333333333333333333333333")};
    coins.AddCoin(COutPoint(h_res, 0), Coin(CTxOut{prev_res}, 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(COutPoint(h_res, 0));
    mtx.vin.emplace_back(COutPoint(h_oru, 0));
    mtx.vout.emplace_back(2 * RES_COIN, g_res_spk); // inflation
    mtx.vout.emplace_back(COIN - 1000, g_origo_spk);

    const CTransaction tx(mtx);
    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(tx, state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-res-in-belowout");
}

namespace {

constexpr CAmount SATURATING_CONVERGENCE_AMOUNT =
    65536 * static_cast<CAmount>(Consensus::CONVERGENCE_DRAW_WINDOW) * Consensus::MIN_CONVERGENCE_AMOUNT;

CScriptWitness SaturatingMintWitness(const uint256& named_txid = uint256::ONE)
{
    return BuildPcorbBodyResMintWitness(named_txid, SATURATING_CONVERGENCE_AMOUNT, uint256::ONE,
        /*proof_height=*/200, /*target_prev=*/0x1d00ffff, uint256());
}

} // namespace

BOOST_AUTO_TEST_CASE(cvg2_is_oru_not_res_and_does_not_increase_res_value_out)
{
    const CScript cvg2 = Consensus::ConvergenceOutputScript();
    BOOST_CHECK(IsConvergenceScript(cvg2));
    BOOST_CHECK(!IsResScript(cvg2));
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(cvg2), ASSET_ORU);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mtx.vout.emplace_back(1000 * COIN, cvg2);
    const CTransaction tx(mtx);
    BOOST_CHECK_EQUAL(tx.GetValueOut(ASSET_RES), 0);
    BOOST_CHECK_EQUAL(tx.GetValueOut(ASSET_ORU), 1000 * COIN);
    BOOST_CHECK_EQUAL(Consensus::GetOutputValueByAsset(tx, ASSET_RES), 0);
}

BOOST_AUTO_TEST_CASE(is_res_script_matches_getvalueout_classification)
{
    const CScript opret = CScript() << OP_RETURN << std::vector<unsigned char>{0xaa};
    BOOST_CHECK(IsResScript(g_res_spk));
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(g_res_spk), ASSET_RES);
    BOOST_CHECK(!IsResScript(g_origo_spk));
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(g_origo_spk), ASSET_ORU);
    BOOST_CHECK(!IsResScript(opret));
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(opret), ASSET_NONE);
}

BOOST_AUTO_TEST_CASE(ordinary_oru_payment_cannot_create_res)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint prev{uint256S("0x01"), 0};
    coins.AddCoin(prev, Coin(CTxOut(20 * COIN, g_origo_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(prev);
    mtx.vout.emplace_back(Consensus::RES_REWARD_AMOUNT, g_res_spk);

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "mixed-asset-output");
}

BOOST_AUTO_TEST_CASE(forged_proof_cannot_create_res_at_checktxinputs)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint prev{uint256S("0x02"), 0};
    coins.AddCoin(prev, Coin(CTxOut(20 * COIN, g_origo_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(prev);
    mtx.vout.emplace_back(Consensus::RES_REWARD_AMOUNT, g_res_spk);
    mtx.vin[0].scriptWitness = BuildPcorbBodyResMintWitness(uint256::ONE, /*amount=*/0, uint256::ONE,
        200, 0x1d00ffff, uint256());

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-res-proof");
}

BOOST_AUTO_TEST_CASE(mint_shape_amount_not_100_rejected_at_checktxinputs)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint prev{uint256S("0x03"), 0};
    coins.AddCoin(prev, Coin(CTxOut(20 * COIN, g_origo_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(prev);
    mtx.vout.emplace_back(Consensus::RES_REWARD_AMOUNT / 2, g_res_spk);
    mtx.vin[0].scriptWitness = SaturatingMintWitness();

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-res-amount");
}

BOOST_AUTO_TEST_CASE(mint_shape_two_res_outputs_rejected_at_checktxinputs)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint prev{uint256S("0x04"), 0};
    coins.AddCoin(prev, Coin(CTxOut(20 * COIN, g_origo_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(prev);
    mtx.vout.emplace_back(Consensus::RES_REWARD_AMOUNT / 2, MakeRes2TestSpk());
    std::vector<unsigned char> wp2(32, 0xcd);
    std::memcpy(wp2.data(), RigorTags::Res2WirePrefix.data(), RigorTags::Res2WirePrefix.size());
    mtx.vout.emplace_back(Consensus::RES_REWARD_AMOUNT / 2, CScript() << OP_1 << wp2);
    mtx.vin[0].scriptWitness = SaturatingMintWitness();

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-res-amount");
}

BOOST_AUTO_TEST_CASE(legal_mint_shape_accepted_at_checktxinputs)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint prev{uint256S("0x05"), 0};
    coins.AddCoin(prev, Coin(CTxOut(20 * COIN, g_origo_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(prev);
    mtx.vout.emplace_back(Consensus::RES_REWARD_AMOUNT, g_res_spk);
    mtx.vin[0].scriptWitness = SaturatingMintWitness();

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(CTransaction(mtx).GetValueOut(ASSET_RES), Consensus::RES_REWARD_AMOUNT);
}

BOOST_AUTO_TEST_CASE(mixed_inputs_cannot_mint_extra_res)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint prev_oru{uint256S("0x06"), 0};
    const COutPoint prev_res{uint256S("0x07"), 0};
    coins.AddCoin(prev_oru, Coin(CTxOut(20 * COIN, g_origo_spk), 100, false), false);
    coins.AddCoin(prev_res, Coin(CTxOut(RES_COIN, g_res_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(prev_res);
    mtx.vin.emplace_back(prev_oru);
    mtx.vout.emplace_back(RES_COIN + Consensus::RES_REWARD_AMOUNT, g_res_spk);
    mtx.vin[0].scriptWitness = SaturatingMintWitness();

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-res-in-belowout");
}

BOOST_AUTO_TEST_CASE(coinbase_res_is_counted_but_not_rejected_by_checktransaction)
{
    // Mockable-chain -reswalletbootstrap pays 1.00 RES on coinbase. CheckTransaction is
    // context-free and cannot see chainparams, so the reject is ConnectBlock
    // `bad-cb-res-amount` (expected 0 unless that backdoor is on).
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << CScriptNum{1} << CScriptNum{1};
    cb.vout.emplace_back(Consensus::RES_REWARD_AMOUNT, g_res_spk);
    const CTransaction tx(cb);
    TxValidationState state;
    BOOST_CHECK(tx.IsCoinBase());
    BOOST_CHECK(CheckTransaction(tx, state));
    BOOST_CHECK_EQUAL(tx.GetValueOut(ASSET_RES), Consensus::RES_REWARD_AMOUNT);
}

BOOST_AUTO_TEST_SUITE_END()
