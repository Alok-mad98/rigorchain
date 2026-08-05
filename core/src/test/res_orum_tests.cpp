// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <consensus/convergenceres.h>
#include <consensus/tx_check.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/rigor_tags.h>
#include <script/script.h>
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

BOOST_AUTO_TEST_SUITE_END()
