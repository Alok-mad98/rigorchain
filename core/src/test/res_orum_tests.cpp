// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <chainparamsbase.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/res_amount.h>
#include <consensus/convergenceres.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergence_root.h>
#include <consensus/res_mint_carrier.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_res.h>
#include <consensus/tx_check.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <node/devtest_res_bootstrap.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/standard.h>
#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <uint256.h>
#include <test/util/setup_common.h>
#include <util/system.h>

#include <cassert>
#include <cstring>
#include <cstdint>
#include <optional>
#include <utility>

#include <boost/test/unit_test.hpp>

namespace {

/** ORU: witness v0-style 20-byte program (not v1 RES). */
const CScript g_origo_spk = CScript() << OP_0 << std::vector<unsigned char>(20, 0x11);

/** RES: witness v1 + 32-byte native Ed25519 program (full pubkey). */
const CScript g_res_spk = CScript() << OP_1 << std::vector<unsigned char>(32, 0xab);

CScript HonestMintSpk(int spend_height)
{
    const std::vector<unsigned char> pk(32, 0x11);
    return GetResMintScriptForProgram(
        static_cast<int64_t>(spend_height) + Params().GetConsensus().nCoinbaseMaturity, pk);
}

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

    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vout.emplace_back(0, CScript() << OP_RETURN);
    const auto ids = Consensus::ListConvergenceRootCommitmentTxids(
        {MakeTransactionRef(cb), MakeTransactionRef(tx)}, coins, 200, Params().GetConsensus());
    BOOST_REQUIRE_EQUAL(ids.size(), 1U);
    BOOST_CHECK_EQUAL(ids[0], tx.GetHash());
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

CTxOut SaturatingCarrier()
{
    return BuildPcorbCarrierOutput(uint256::ONE, SATURATING_CONVERGENCE_AMOUNT, uint256::ONE,
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
    mtx.vout.emplace_back(Consensus::RES_REWARD_GENESIS, g_res_spk);

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "mixed-asset-output");
}

BOOST_AUTO_TEST_CASE(body_tx_with_pcorb_carrier_is_ordinary_op_return)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint prev{uint256S("0x02"), 0};
    coins.AddCoin(prev, Coin(CTxOut(20 * COIN, g_origo_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(prev);
    mtx.vout.emplace_back(19 * COIN, g_origo_spk);
    mtx.vout.push_back(SaturatingCarrier());

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
}

BOOST_AUTO_TEST_CASE(leftover_witness_cannot_create_res_at_checktxinputs)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint prev{uint256S("0x02"), 0};
    coins.AddCoin(prev, Coin(CTxOut(20 * COIN, g_origo_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(prev);
    mtx.vout.emplace_back(Consensus::RES_REWARD_GENESIS, g_res_spk);
    mtx.vin[0].scriptWitness.stack.assign(6, std::vector<unsigned char>(32, 0));

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "mixed-asset-output");
}

BOOST_AUTO_TEST_CASE(legal_body_mint_shape_rejected_at_checktxinputs)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint prev{uint256S("0x05"), 0};
    coins.AddCoin(prev, Coin(CTxOut(20 * COIN, g_origo_spk), 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.emplace_back(prev);
    mtx.vout.emplace_back(Consensus::RES_REWARD_GENESIS, HonestMintSpk(200));
    mtx.vout.push_back(SaturatingCarrier());

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "mixed-asset-output");
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
    mtx.vout.emplace_back(RES_COIN + Consensus::RES_REWARD_GENESIS, g_res_spk);

    TxValidationState state;
    CAmount fee{};
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, 200, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-res-in-belowout");
}

BOOST_AUTO_TEST_CASE(coinbase_res_is_counted_but_not_rejected_by_checktransaction)
{
    // Mockable-chain Consensus::Params::res_wallet_bootstrap_coinbase (from
    // -reswalletbootstrap at CreateChainParams) pays 1.000000 RES on coinbase.
    // CheckTransaction is context-free and cannot see chainparams, so the reject
    // is ConnectBlock `bad-cb-res-amount` (expected 0 unless that field is set).
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << CScriptNum{1} << CScriptNum{1};
    cb.vout.emplace_back(Consensus::RES_REWARD_GENESIS, g_res_spk);
    const CTransaction tx(cb);
    TxValidationState state;
    BOOST_CHECK(tx.IsCoinBase());
    BOOST_CHECK(CheckTransaction(tx, state));
    BOOST_CHECK_EQUAL(tx.GetValueOut(ASSET_RES), Consensus::RES_REWARD_GENESIS);
}

BOOST_AUTO_TEST_CASE(res_mint_op_res_mint_encodes_maturity_and_owner)
{
    const Consensus::Params& params = Params().GetConsensus();
    BOOST_CHECK_EQUAL(params.nCoinbaseMaturity, COINBASE_MATURITY);
    BOOST_CHECK_EQUAL(RES_MINT_MATURITY, COINBASE_MATURITY);

    const std::vector<unsigned char> pk(32, 0x11);
    const uint160 pkh = Hash160(pk);
    const int64_t mint_height = 50;
    const int64_t maturity_height = mint_height + params.nCoinbaseMaturity;
    const CScript spk = GetResMintScript(maturity_height, pkh);

    BOOST_CHECK(IsResMintScript(spk));
    BOOST_CHECK(IsResScript(spk));
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(spk), ASSET_RES);
    BOOST_CHECK(Consensus::IsAllowedScript(spk));

    int64_t parsed_mat = 0;
    std::vector<unsigned char> parsed_pkh;
    BOOST_CHECK(ParseResMintScript(spk, parsed_mat, parsed_pkh));
    BOOST_CHECK_EQUAL(parsed_mat, maturity_height);
    BOOST_CHECK(parsed_pkh.size() == 20);
    BOOST_CHECK(uint160(parsed_pkh) == pkh);

    // Dummy detached Ed25519 sig (64 bytes + hashtype). VerifyScript admits the
    // OP_RES_MINT template (witness v0/v1 are not required). BaseSignatureChecker
    // reports spend height 0, so maturity fails before the dummy signature is checked.
    std::vector<unsigned char> sig(ed25519::SIGNATURE_SIZE + 1, 0x22);
    sig.back() = SIGHASH_ALL;
    const CScript scriptSig = CScript() << sig << pk;

    ScriptError verr{SCRIPT_ERR_OK};
    BOOST_CHECK(!VerifyScript(scriptSig, spk, nullptr, SCRIPT_VERIFY_NONE,
                              BaseSignatureChecker(), &verr));
    BOOST_CHECK_EQUAL(verr, SCRIPT_ERR_UNSATISFIED_LOCKTIME);

    class ResMintTestChecker : public BaseSignatureChecker
    {
        int m_height;
        bool m_sig_ok;
    public:
        ResMintTestChecker(int height, bool sig_ok) : m_height(height), m_sig_ok(sig_ok) {}
        int GetSpendHeight() const override { return m_height; }
        bool CheckEd25519Signature(const std::vector<unsigned char>&, const std::vector<unsigned char>&,
                                   const CScript&, SigVersion) const override
        {
            return m_sig_ok;
        }
    };

    auto eval_mint = [&](const CScript& mint_spk, const BaseSignatureChecker& checker, ScriptError& err) {
        std::vector<std::vector<unsigned char>> stack;
        err = SCRIPT_ERR_OK;
        if (!EvalScript(stack, scriptSig, SCRIPT_VERIFY_NONE, checker, SigVersion::BASE, &err)) {
            return false;
        }
        if (!EvalScript(stack, mint_spk, SCRIPT_VERIFY_NONE, checker, SigVersion::BASE, &err)) {
            return false;
        }
        if (stack.empty() || stack.back().empty() || stack.back()[0] == 0) {
            err = SCRIPT_ERR_EVAL_FALSE;
            return false;
        }
        return true;
    };

    ScriptError err{SCRIPT_ERR_OK};
    BOOST_CHECK(!eval_mint(spk, ResMintTestChecker(static_cast<int>(maturity_height) - 1, true), err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_UNSATISFIED_LOCKTIME);

    err = SCRIPT_ERR_OK;
    BOOST_CHECK(eval_mint(spk, ResMintTestChecker(static_cast<int>(maturity_height), true), err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);

    const CScript wrong_owner = GetResMintScript(maturity_height, uint160());
    err = SCRIPT_ERR_OK;
    BOOST_CHECK(!eval_mint(wrong_owner, ResMintTestChecker(static_cast<int>(maturity_height), true), err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_EQUALVERIFY);
}

BOOST_AUTO_TEST_CASE(res_transfer_not_subject_to_mint_maturity)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    coins.AddCoin(COutPoint(uint256{uint8_t{1}}, 0), Coin(CTxOut(Consensus::RES_REWARD_GENESIS, g_res_spk), /*nHeight=*/50, false), false);
    coins.AddCoin(COutPoint(uint256{uint8_t{2}}, 0), Coin(CTxOut(COIN, g_origo_spk), /*nHeight=*/1, false), false);

    CMutableTransaction mtx;
    mtx.vin.resize(2);
    mtx.vin[0].prevout = COutPoint(uint256{uint8_t{2}}, 0);
    mtx.vin[1].prevout = COutPoint(uint256{uint8_t{1}}, 0);
    mtx.vout.resize(2);
    mtx.vout[0].nValue = COIN;
    mtx.vout[0].scriptPubKey = g_origo_spk;
    mtx.vout[1].nValue = Consensus::RES_REWARD_GENESIS;
    mtx.vout[1].scriptPubKey = g_res_spk;

    CAmount fee = 0;
    TxValidationState state;
    BOOST_CHECK(Consensus::CheckTxInputs(CTransaction(mtx), state, coins, /*nSpendHeight=*/51, Params().GetConsensus(), fee));
}

BOOST_AUTO_TEST_CASE(oru_coinbase_still_immature)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    coins.AddCoin(COutPoint(uint256{uint8_t{1}}, 0), Coin(CTxOut(COIN, g_origo_spk), /*nHeight=*/50, /*fCoinBaseIn=*/true), false);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256{uint8_t{1}}, 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = COIN;
    mtx.vout[0].scriptPubKey = g_origo_spk;

    CAmount fee = 0;
    TxValidationState state;
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(mtx), state, coins, /*nSpendHeight=*/51, Params().GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-premature-spend-of-coinbase");
}

BOOST_AUTO_TEST_CASE(w11_res_wallet_bootstrap_injected_at_chain_construction)
{
    {
        ArgsManager args;
        args.ForceSetArg("-reswalletbootstrap", "1");
        const auto regtest = CreateChainParams(args, CBaseChainParams::REGTEST);
        BOOST_CHECK(regtest->IsMockableChain());
        BOOST_CHECK_EQUAL(regtest->GetConsensus().res_wallet_bootstrap_coinbase, RES_COIN);
        BOOST_CHECK_EQUAL(DevTestResWalletBootstrap(*regtest), RES_COIN);

        const auto devtest = CreateChainParams(args, CBaseChainParams::DEVTEST);
        BOOST_CHECK_EQUAL(devtest->GetConsensus().res_wallet_bootstrap_coinbase, RES_COIN);

        const auto slowtest = CreateChainParams(args, CBaseChainParams::SLOWTEST);
        BOOST_CHECK_EQUAL(slowtest->GetConsensus().res_wallet_bootstrap_coinbase, RES_COIN);

        // Production nets ignore the CLI flag: amount stays 0.
        const auto main = CreateChainParams(args, CBaseChainParams::MAIN);
        BOOST_CHECK(!main->IsMockableChain());
        BOOST_CHECK_EQUAL(main->GetConsensus().res_wallet_bootstrap_coinbase, 0);
        BOOST_CHECK_EQUAL(DevTestResWalletBootstrap(*main), 0);
    }
    {
        ArgsManager args;
        const auto regtest = CreateChainParams(args, CBaseChainParams::REGTEST);
        BOOST_CHECK_EQUAL(regtest->GetConsensus().res_wallet_bootstrap_coinbase, 0);
        BOOST_CHECK_EQUAL(DevTestResWalletBootstrap(*regtest), 0);
    }
    {
        CChainParams::RegTestOptions opts;
        opts.res_wallet_bootstrap_coinbase = RES_COIN;
        const auto injected = CChainParams::RegTest(opts);
        BOOST_CHECK_EQUAL(injected->GetConsensus().res_wallet_bootstrap_coinbase, RES_COIN);
        BOOST_CHECK_EQUAL(DevTestResWalletBootstrap(*injected), RES_COIN);
    }
}

/**
 * OP_CVG_CLAIM (0xbc) is retired, not merely gated: the opcode, ASSET_CVG, TxoutType::CVG_CLAIM
 * and both covenant shape checks are gone. A ticket could not authorize its own draw win, so a
 * covenant-funded mint needed the same ConvergeProof as an ORU-funded one and bought nothing,
 * while costing a third asset class that was neither circulating ORU nor destroyed ORU.
 *
 * This pins the whole surface being absent, so reintroducing the shape fails loudly instead of
 * quietly re-entering consensus behind a gate.
 */
BOOST_AUTO_TEST_CASE(op_cvg_claim_is_retired)
{
    const CScript retired = CScript() << std::vector<unsigned char>(20, 0x11)
                                      << static_cast<opcodetype>(0xbc);

    // 0xbc is burned (see script.h): HasValidOps rejects it even though MAX_OPCODE is now OP_CONVERGE.
    BOOST_CHECK_EQUAL(GetOpName(static_cast<opcodetype>(0xbc)), std::string("OP_UNKNOWN"));
    BOOST_CHECK(!retired.HasValidOps());

    // No covenant branch in the classifier, so the shape falls through to the legacy
    // non-witness default and consensus refuses it outright rather than gating it.
    std::vector<std::vector<unsigned char>> solutions;
    BOOST_CHECK_EQUAL(ClassifyRigorTxout(retired, solutions), TxoutType::NONSTANDARD);
    BOOST_CHECK(!Consensus::IsAllowedScript(retired));

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mtx.vout.emplace_back(1000 * COIN, retired);
    TxValidationState state;
    BOOST_CHECK(!CheckTransaction(CTransaction(mtx), state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-script-type");

    TxoutType which{TxoutType::NONSTANDARD};
    BOOST_CHECK(!::IsStandard(retired, std::nullopt, which));

    // Executing it is a bad opcode, not a covenant check.
    std::vector<std::vector<unsigned char>> stack{{0x01}, {0x01}, {0x01}};
    ScriptError err{SCRIPT_ERR_OK};
    BOOST_CHECK(!EvalScript(stack, retired, SCRIPT_VERIFY_NONE, BaseSignatureChecker(),
                            SigVersion::BASE, &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_BAD_OPCODE);

    // OP_CONVERGE is the only convergence form, which is what makes draw weight and RFC-002 C(t)
    // the same number: the output never enters the UTXO set, so committing it as destroyed
    // ORU is sound. The retired 0xbc shape carries no draw weight at all.
    CMutableTransaction cvg2;
    cvg2.vout.emplace_back(1000 * COIN, Consensus::ConvergenceOutputScript());
    const CTransaction cvg2_tx(cvg2);
    BOOST_CHECK(Consensus::IsConvergenceTx(cvg2_tx));
    BOOST_CHECK_EQUAL(Consensus::GetConvergenceAmount(cvg2_tx), 1000 * COIN);
    BOOST_CHECK_EQUAL(GetTransactionConvergenceAmount(cvg2_tx),
                      static_cast<uint64_t>(1000 * COIN));

    CMutableTransaction ticket;
    ticket.vout.emplace_back(1000 * COIN, retired);
    const CTransaction ticket_tx(ticket);
    BOOST_CHECK(!Consensus::IsConvergenceTx(ticket_tx));
    BOOST_CHECK_EQUAL(Consensus::CountConvergenceOutputs(ticket_tx), 0);
    BOOST_CHECK_EQUAL(Consensus::GetConvergenceAmount(ticket_tx), 0);
}

BOOST_AUTO_TEST_CASE(parse_res_mint_rejects_noncanonical_encoding)
{
    const uint160 pkh;
    const CScript canonical = GetResMintScript(/*maturity_height=*/5, pkh);
    int64_t mat = 0;
    std::vector<unsigned char> parsed_pkh;
    BOOST_CHECK(ParseResMintScript(canonical, mat, parsed_pkh));
    BOOST_CHECK_EQUAL(mat, 5);

    CScript pushdata1;
    pushdata1.push_back(OP_PUSHDATA1);
    pushdata1.push_back(1);
    pushdata1.push_back(5);
    pushdata1 << ToByteVector(pkh) << OP_RES_MINT;
    BOOST_CHECK(pushdata1 != canonical);
    BOOST_CHECK(!ParseResMintScript(pushdata1, mat, parsed_pkh));
}

BOOST_AUTO_TEST_CASE(res2_prefix_is_not_an_allowed_script)
{
    std::vector<unsigned char> wp(32, 0xab);
    std::memcpy(wp.data(), RigorTags::Res2WirePrefix.data(), RigorTags::Res2WirePrefix.size());
    const CScript res2 = CScript() << OP_1 << wp;
    BOOST_CHECK(!Consensus::IsAllowedScript(res2));
    BOOST_CHECK(!IsResScript(res2));
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(res2), ASSET_NONE);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mtx.vout.emplace_back(RES_ATOMIC_UNIT, res2);
    TxValidationState state;
    BOOST_CHECK(!CheckTransaction(CTransaction(mtx), state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-script-type");
}

BOOST_AUTO_TEST_CASE(res_mint_scriptsig_must_be_push_only)
{
    const uint160 pkh;
    const CScript spk = GetResMintScript(/*maturity_height=*/0, pkh);
    std::vector<unsigned char> sig(ed25519::SIGNATURE_SIZE + 1, 0x22);
    sig.back() = SIGHASH_ALL;
    const std::vector<unsigned char> pk(32, 0x11);
    CScript malleated;
    malleated << OP_DROP << sig << pk;
    ScriptError err{SCRIPT_ERR_OK};
    BOOST_CHECK(!VerifyScript(malleated, spk, nullptr, SCRIPT_VERIFY_NONE,
                              BaseSignatureChecker(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_SIG_PUSHONLY);
}

BOOST_AUTO_TEST_SUITE_END()
