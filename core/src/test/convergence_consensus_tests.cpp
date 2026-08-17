// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <coins.h>
#include <consensus/rigor_math.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergence_root.h>
#include <consensus/draw_digest.h>
#include <consensus/convergenceres.h>
#include <consensus/res_owner.h>
#include <consensus/res_proof.h>
#include <consensus/res_mint_witness.h>
#include <consensus/res_amount.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <consensus/rigor_math.h>
#include <crypto/common.h>
#include <hash.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <undo.h>

#include <boost/test/unit_test.hpp>
#include <set>

namespace {
/** Same CVG2 prefix as ConvergenceOutputScript(); differs in tail so txid changes (no OP_RETURN convergence path). */
CScript MakeconvergenceCvg2AlternateProgram()
{
    std::vector<unsigned char> wp(32, 0);
    wp[0] = 0x43;
    wp[1] = 0x56;
    wp[2] = 0x47;
    wp[3] = 0x32;
    wp[31] = 0x01;
    return CScript() << OP_1 << wp;
}

const CScript g_oru_spk = CScript() << OP_0 << std::vector<unsigned char>(20, 0x22);

/** RES payout script for an owner program: `OP_1 <program32>` (witness v1, not CVG2-tagged). */
CScript MakeResPayoutScript(unsigned char fill)
{
    return CScript() << OP_1 << std::vector<unsigned char>(32, fill);
}

/** SECURITY-001-A / A-5 R1: draw fixtures must declare an owner or they never enter F. */
CTxOut MakeOwnerDeclarationOut()
{
    return CTxOut(0, Consensus::ResOwnerDeclarationScript(std::vector<unsigned char>(32, 0xa1)));
}

/**
 * A convergence amount that satisfies H < T·f for every possible σ, so cases about determinism,
 * ordering or argmin never have to reason about draw odds.
 *
 * A-9-C divided T by the window, which multiplied this saturation point by the same factor:
 * T·f ≥ 2^256 now needs f ≥ 65536 · W · MIN_CONVERGENCE_AMOUNT. Derived rather than written out
 * so that changing W cannot silently turn these fixtures probabilistic.
 */
constexpr CAmount SATURATING_CONVERGENCE_AMOUNT =
    65536 * static_cast<CAmount>(Consensus::CONVERGENCE_DRAW_WINDOW) * Consensus::MIN_CONVERGENCE_AMOUNT;
} // namespace

BOOST_FIXTURE_TEST_SUITE(convergence_consensus_tests, BasicTestingSetup)

// SECURITY-001-A / A-5 R2: the RES minted for a convergence must pay the owner that
// convergence declared, not a script the block producer chose.
BOOST_AUTO_TEST_CASE(res_reward_owner_binding)
{
    const std::vector<unsigned char> owner_program(32, 0xa1);

    CMutableTransaction conv;
    conv.vin.resize(1);
    conv.vin[0].prevout = COutPoint(uint256S("0x11"), 0);
    conv.vout.resize(2);
    conv.vout[0].nValue = COIN;
    conv.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
    conv.vout[1] = CTxOut(0, Consensus::ResOwnerDeclarationScript(owner_program));
    const auto conv_ref = MakeTransactionRef(conv);

    // SECURITY-001-A8: the declared program reaches the check via the parent block's draw set,
    // so the check no longer scans a block for the winner — it is handed the program directly.
    const auto declared = Consensus::GetResOwnerDeclaration(*conv_ref);
    BOOST_REQUIRE(declared.has_value());
    BOOST_CHECK(declared->program32 == owner_program);

    auto mint_paying = [](const CScript& spk) {
        CMutableTransaction mint;
        mint.vin.resize(1);
        mint.vout.resize(1);
        mint.vout[0].nValue = 1;
        mint.vout[0].scriptPubKey = spk;
        return CTransaction(mint);
    };

    // Pays the declared owner.
    BOOST_CHECK(Consensus::CheckResRewardOwner(owner_program, mint_paying(MakeResPayoutScript(0xa1))));

    // The A-5 attack: a producer redirecting the reward to its own script.
    BOOST_CHECK(!Consensus::CheckResRewardOwner(owner_program, mint_paying(MakeResPayoutScript(0xbb))));

    // No RES output at all.
    CMutableTransaction no_res;
    no_res.vin.resize(1);
    no_res.vout.resize(1);
    no_res.vout[0] = CTxOut(0, g_oru_spk);
    BOOST_CHECK(!Consensus::CheckResRewardOwner(owner_program, CTransaction(no_res)));

    // Two RES outputs would let a producer pay the owner a token amount and keep the rest.
    CMutableTransaction two_res;
    two_res.vin.resize(1);
    two_res.vout.resize(2);
    two_res.vout[0] = CTxOut(1, MakeResPayoutScript(0xa1));
    two_res.vout[1] = CTxOut(1, MakeResPayoutScript(0xbb));
    BOOST_CHECK(!Consensus::CheckResRewardOwner(owner_program, CTransaction(two_res)));

    // An unset program cannot authorize a payout; R1 guarantees every member of F has one, so
    // reaching here at all means the draw set was malformed and the mint must fail closed.
    BOOST_CHECK(!Consensus::CheckResRewardOwner({}, mint_paying(MakeResPayoutScript(0xa1))));
}

BOOST_AUTO_TEST_CASE(convergence_two_cvg2_shapes_same_amount_and_flags)
{
    const CAmount burn = COIN;

    CCoinsView base;
    CCoinsViewCache coins(&base);
    CTxOut prev_oru(20 * COIN, g_oru_spk);
    const uint256 h_oru{uint256S("0xdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut{prev_oru}, 100, false), false);

    auto make_mtx = [&](const CScript& convergence_spk) {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = COutPoint(h_oru, 0);
        mtx.vout.resize(2);
        mtx.vout[0].nValue = burn;
        mtx.vout[0].scriptPubKey = convergence_spk;
        mtx.vout[1].nValue = 19 * COIN;
        mtx.vout[1].scriptPubKey = g_oru_spk;
        return mtx;
    };

    const CTransaction tx_default(make_mtx(Consensus::ConvergenceOutputScript()));
    const CTransaction tx_alt(make_mtx(MakeconvergenceCvg2AlternateProgram()));

    BOOST_CHECK(Consensus::IsConvergenceTx(tx_default));
    BOOST_CHECK(Consensus::IsConvergenceTx(tx_alt));
    BOOST_CHECK_EQUAL(Consensus::GetConvergenceAmount(tx_default), burn);
    BOOST_CHECK_EQUAL(Consensus::GetConvergenceAmount(tx_alt), burn);
    BOOST_CHECK_EQUAL(Consensus::GetConvergenceAmount(tx_default), Consensus::GetConvergenceAmount(tx_alt));

    TxValidationState st_a, st_b;
    CAmount fee_a{}, fee_b{};
    BOOST_CHECK(Consensus::CheckTxInputs(tx_default, st_a, coins, 200, Params().GetConsensus(), fee_a));
    BOOST_CHECK(Consensus::CheckTxInputs(tx_alt, st_b, coins, 200, Params().GetConsensus(), fee_b));
    BOOST_CHECK_EQUAL(fee_a, fee_b);
}

BOOST_AUTO_TEST_CASE(convergence_draw_candidate_hash_depends_on_txid_not_script_shape)
{
    const CAmount burn = COIN;

    CCoinsView base;
    CCoinsViewCache coins(&base);
    CTxOut prev_oru(20 * COIN, g_oru_spk);
    const uint256 h_oru{uint256S("0xeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut{prev_oru}, 100, false), false);

    auto make_mtx = [&](const CScript& convergence_spk) {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = COutPoint(h_oru, 0);
        mtx.vout.resize(2);
        mtx.vout[0].nValue = burn;
        mtx.vout[0].scriptPubKey = convergence_spk;
        mtx.vout[1].nValue = 19 * COIN;
        mtx.vout[1].scriptPubKey = g_oru_spk;
        return mtx;
    };

    const CTransaction tx_default(make_mtx(Consensus::ConvergenceOutputScript()));
    const CTransaction tx_alt(make_mtx(MakeconvergenceCvg2AlternateProgram()));

    const uint256 bind{uint256S("0x1111111111111111111111111111111111111111111111111111111111111111")};
    const int block_h = 200;

    // Same burn (vout value); different CVG2 program tail → different txid → different draw h.
    BOOST_CHECK(tx_default.GetHash() != tx_alt.GetHash());
    const uint256 convergence_root{};
    BOOST_CHECK(SelectionDigest(tx_default.GetHash(), bind, block_h, convergence_root) !=
                SelectionDigest(tx_alt.GetHash(), bind, block_h, convergence_root));

    const uint256 synthetic_txid = uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    const uint256 h_once = SelectionDigest(synthetic_txid, bind, block_h, convergence_root);
    BOOST_CHECK(h_once == SelectionDigest(synthetic_txid, bind, block_h, convergence_root));
}

BOOST_AUTO_TEST_CASE(convergence_sigma_candidate_easy_target)
{
    const CAmount burn = COIN;

    CCoinsView base;
    CCoinsViewCache coins(&base);
    CTxOut prev_oru(20 * COIN, g_oru_spk);
    const uint256 h_oru{uint256S("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff00")};
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut{prev_oru}, 100, false), false);

    auto make_mtx = [&](const CScript& convergence_spk, bool declare_owner) {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = COutPoint(h_oru, 0);
        mtx.vout.resize(2);
        mtx.vout[0].nValue = burn;
        mtx.vout[0].scriptPubKey = convergence_spk;
        // Keep fee > 0 so the tx is a valid convergence candidate under V2.2.
        mtx.vout[1].nValue = 19 * COIN - 1;
        mtx.vout[1].scriptPubKey = g_oru_spk;
        if (declare_owner) mtx.vout.push_back(MakeOwnerDeclarationOut());
        return mtx;
    };

    const int block_h = 200;
    BOOST_CHECK(burn >= Consensus::MIN_CONVERGENCE_AMOUNT);

    // Candidacy for F must not depend on which CVG2 program shape the convergence used. The
    // draw itself is keyed by txid, which necessarily differs between the two shapes, so parity
    // is asserted here on admission rather than on the σ outcome.
    for (bool declare_owner : {false, true}) {
        const CTransaction tx_default(make_mtx(Consensus::ConvergenceOutputScript(), declare_owner));
        const CTransaction tx_alt(make_mtx(MakeconvergenceCvg2AlternateProgram(), declare_owner));

        BOOST_CHECK_EQUAL(IsConvergenceCandidate(tx_default), IsConvergenceCandidate(tx_alt));
        const CAmount fee_default = ComputeConvergenceFee(tx_default, coins, block_h, Params().GetConsensus());
        const CAmount fee_alt = ComputeConvergenceFee(tx_alt, coins, block_h, Params().GetConsensus());
        BOOST_CHECK_EQUAL(fee_default, fee_alt);
        // A-5 R1: without a declared owner the convergence never enters F, whatever its shape.
        BOOST_CHECK_EQUAL(fee_default > 0, declare_owner);
    }
}

BOOST_AUTO_TEST_CASE(convergence_extract_amount_big_endian_and_compute_convergence_hash_alias)
{
    std::vector<unsigned char> wp(32, 0);
    wp[0] = 'C';
    wp[1] = 'V';
    wp[2] = 'G';
    wp[3] = '2';
    const CAmount expect_amt = COIN;
    const uint64_t ev = static_cast<uint64_t>(expect_amt);
    for (int i = 0; i < 8; ++i) {
        wp[4 + i] = static_cast<unsigned char>((ev >> (56 - 8 * i)) & 0xff);
    }
    const CScript spk = CScript() << OP_1 << wp;
    CAmount dec{};
    BOOST_REQUIRE(ExtractConvergenceAmount(spk, dec));
    BOOST_CHECK_EQUAL(dec, expect_amt);

    const uint256 txid = uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    const uint256 bind = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    const int h = 42;
    const uint256 convergence_root{};
    BOOST_CHECK(ComputeConvergenceHash(txid, bind, h, convergence_root) == SelectionDigest(txid, bind, h, convergence_root));
}

BOOST_AUTO_TEST_CASE(fee_domain_uses_inputs_not_first_vout_op_return)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const uint256 h_oru{uint256S("0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcd")};
    CTxOut prev_oru(20 * COIN, g_oru_spk);
    coins.AddCoin(COutPoint(h_oru, 0), Coin(CTxOut{prev_oru}, 100, false), false);

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(h_oru, 0);
    mtx.vout.resize(2);
    mtx.vout[0].nValue = 0;
    mtx.vout[0].scriptPubKey = CScript() << OP_RETURN << std::vector<unsigned char>{1, 2, 3};
    mtx.vout[1].nValue = 19 * COIN;
    mtx.vout[1].scriptPubKey = g_oru_spk;
    const CTransaction tx(mtx);

    const std::optional<AssetType> dom = Consensus::GetTransactionInputAsset(tx, coins);
    BOOST_REQUIRE(dom.has_value());
    BOOST_CHECK(*dom == ASSET_ORU);
}

BOOST_AUTO_TEST_CASE(collect_winner_empty_body_matches_empty_draw_candidate_root)
{
    CBlock block;
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << CScriptNum{1};
    cb.vout.emplace_back(0, CScript() << OP_RETURN);
    block.vtx.push_back(MakeTransactionRef(cb));

    const int block_h = 5;

    CCoinsView empty_base;
    CCoinsViewCache view(&empty_base);
    const std::optional<uint256> w = GetBlockWinner(block, block_h, view, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    BOOST_CHECK(!w.has_value());
    const uint256 root = ComputeConvergenceDrawCandidateRoot(block, block_h, view, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    BOOST_CHECK_EQUAL(root, ComputeConvergenceDrawMerkleRoot({}));
}

//BOOST_AUTO_TEST_CASE(convergence_beta_weight_nonzero)
//{
//    const arith_uint256 w = Consensus::ConvergenceAmountBetaWeight(COIN);
//    BOOST_CHECK(w > arith_uint256(0));
//}

//BOOST_AUTO_TEST_CASE(rigor_hash_single_source_matches_draw_and_proof)
//{
//    const uint256 txid = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
//    const uint256 bind = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
//    const uint64_t height = 123456789ULL;
//    const uint32_t target_prev = 0x1f00ffff;
//    const uint256 convergence_root{};
//    const uint256 h0 = Consensus::ComputeRigorHash(txid, bind, height, target_prev, convergence_root);
//    BOOST_CHECK(h0 == Consensus::DrawDigest(txid, bind, height, target_prev, convergence_root));
//    BOOST_CHECK(h0 == Consensus::ConvergeProofDigest(txid, bind, height, target_prev, convergence_root));
//}

BOOST_AUTO_TEST_CASE(convergence_winner_is_deterministic_on_repeat_and_undo_path)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const COutPoint prev0{uint256S("0xaaaabbbb00000000000000000000000000000000000000000000000000000000"), 0};
    const COutPoint prev1{uint256S("0xaaaabbbb11111111111111111111111111111111111111111111111111111111"), 0};
    coins.AddCoin(prev0, Coin(CTxOut{SATURATING_CONVERGENCE_AMOUNT + 2 * COIN, g_oru_spk}, 100, false), false);
    coins.AddCoin(prev1, Coin(CTxOut{2 * SATURATING_CONVERGENCE_AMOUNT + 2 * COIN, g_oru_spk}, 100, false), false);

    auto make_convergence_tx = [&](const COutPoint& prev, CAmount convergence_amt, CAmount change_amt) {
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vin[0].prevout = prev;
        mtx.vout.resize(2);
        mtx.vout[0].nValue = convergence_amt;
        mtx.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
        mtx.vout[1].nValue = change_amt;
        mtx.vout[1].scriptPubKey = g_oru_spk;
        mtx.vout.push_back(MakeOwnerDeclarationOut());
        return CTransaction{mtx};
    };

    // This case is about determinism of the winner across repeat and undo evaluation, not about
    // the draw odds, so both candidates converge past the saturation point and are eligible with
    // certainty. Weights stay distinct. Fees stay at 1 ORU each.
    const CTransaction tx_a = make_convergence_tx(prev0, SATURATING_CONVERGENCE_AMOUNT, COIN);
    const CTransaction tx_b = make_convergence_tx(prev1, 2 * SATURATING_CONVERGENCE_AMOUNT, COIN);

    CBlock block;
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << CScriptNum{1};
    cb.vout.emplace_back(0, CScript() << OP_RETURN);
    block.vtx.push_back(MakeTransactionRef(cb));
    block.vtx.push_back(MakeTransactionRef(tx_a));
    block.vtx.push_back(MakeTransactionRef(tx_b));

    // SECURITY-001-A8: the seed is the block's own hash, so the winner is a function of the
    // block alone — no bind or target_prev to supply.
    const int block_h = 200;

    const auto w1 = GetBlockWinner(block, block_h, coins, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    const auto w2 = GetBlockWinner(block, block_h, coins, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    BOOST_REQUIRE(w1.has_value());
    BOOST_CHECK(w1 == w2);

    CBlockUndo undo;
    undo.vtxundo.resize(2);
    undo.vtxundo[0].vprevout.push_back(Coin(CTxOut{SATURATING_CONVERGENCE_AMOUNT + 2 * COIN, g_oru_spk}, 100, false));
    undo.vtxundo[1].vprevout.push_back(Coin(CTxOut{2 * SATURATING_CONVERGENCE_AMOUNT + 2 * COIN, g_oru_spk}, 100, false));
    const auto w_undo = GetBlockWinnerFromBlockUndo(block, block_h, undo, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    BOOST_CHECK(w1 == w_undo);
}

BOOST_AUTO_TEST_CASE(convergence_winner_invariant_under_tx_order_permutation)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint prev0{uint256S("0xbbbbcccc00000000000000000000000000000000000000000000000000000000"), 0};
    const COutPoint prev1{uint256S("0xbbbbcccc11111111111111111111111111111111111111111111111111111111"), 0};
    coins.AddCoin(prev0, Coin(CTxOut{SATURATING_CONVERGENCE_AMOUNT + 2 * COIN, g_oru_spk}, 100, false), false);
    coins.AddCoin(prev1, Coin(CTxOut{SATURATING_CONVERGENCE_AMOUNT + 2 * COIN, g_oru_spk}, 100, false), false);

    // Converge past the saturation point so both candidates are eligible with certainty; this
    // case isolates ordering invariance, not the draw odds.
    auto mk = [&](const COutPoint& prev) {
        CMutableTransaction m;
        m.vin.resize(1);
        m.vin[0].prevout = prev;
        m.vout.resize(2);
        m.vout[0].nValue = SATURATING_CONVERGENCE_AMOUNT;
        m.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
        m.vout[1].nValue = COIN; // fee = 1*COIN
        m.vout[1].scriptPubKey = g_oru_spk;
        m.vout.push_back(MakeOwnerDeclarationOut());
        return CTransaction{m};
    };
    const CTransaction tx0 = mk(prev0);
    const CTransaction tx1 = mk(prev1);

    auto mk_block = [&](const CTransaction& a, const CTransaction& b) {
        CBlock blk;
        CMutableTransaction cb;
        cb.vin.resize(1);
        cb.vin[0].prevout.SetNull();
        cb.vin[0].scriptSig = CScript() << CScriptNum{1};
        cb.vout.emplace_back(0, CScript() << OP_RETURN);
        blk.vtx.push_back(MakeTransactionRef(cb));
        blk.vtx.push_back(MakeTransactionRef(a));
        blk.vtx.push_back(MakeTransactionRef(b));
        return blk;
    };

    const int h = 220;
    const CBlock blk_a = mk_block(tx0, tx1);
    const CBlock blk_b = mk_block(tx1, tx0);
    const uint256 fr_a = Consensus::ComputeConvergenceRootFromConvergenceSet(blk_a.vtx, coins, h, Params().GetConsensus());
    const uint256 fr_b = Consensus::ComputeConvergenceRootFromConvergenceSet(blk_b.vtx, coins, h, Params().GetConsensus());
    BOOST_CHECK(fr_a == fr_b);
    // Both permutations carry the same transactions and therefore the same header, so the σ
    // seed is identical too and only the enumeration order differs.
    BOOST_CHECK(blk_a.GetHash() == blk_b.GetHash());
    const auto w_a = GetBlockWinner(blk_a, h, coins, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    const auto w_b = GetBlockWinner(blk_b, h, coins, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    BOOST_REQUIRE(w_a.has_value());
    BOOST_REQUIRE(w_b.has_value());
    BOOST_CHECK(*w_a == *w_b);
}

BOOST_AUTO_TEST_CASE(winner_is_strict_argmin)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);

    const COutPoint p0{uint256S("0x1000000000000000000000000000000000000000000000000000000000000000"), 0};
    const COutPoint p1{uint256S("0x2000000000000000000000000000000000000000000000000000000000000000"), 0};
    const COutPoint p2{uint256S("0x3000000000000000000000000000000000000000000000000000000000000000"), 0};
    coins.AddCoin(p0, Coin(CTxOut{SATURATING_CONVERGENCE_AMOUNT + 2 * COIN, g_oru_spk}, 100, false), false);
    coins.AddCoin(p1, Coin(CTxOut{2 * SATURATING_CONVERGENCE_AMOUNT + 2 * COIN, g_oru_spk}, 100, false), false);
    coins.AddCoin(p2, Coin(CTxOut{3 * SATURATING_CONVERGENCE_AMOUNT + 2 * COIN, g_oru_spk}, 100, false), false);

    auto mk = [&](const COutPoint& prev, CAmount convergence_amt, CAmount change_amt) {
        CMutableTransaction m;
        m.vin.resize(1);
        m.vin[0].prevout = prev;
        m.vout.resize(2);
        m.vout[0].nValue = convergence_amt;
        m.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
        m.vout[1].nValue = change_amt;
        m.vout[1].scriptPubKey = g_oru_spk;
        m.vout.push_back(MakeOwnerDeclarationOut());
        return CTransaction{m};
    };

    // All three converge past the saturation point, so the eligibility filter admits every
    // candidate deterministically and the case isolates the argmin rule. Weights stay distinct so
    // the H_i·f_j < H_j·f_i comparison is exercised.
    const CTransaction tx0 = mk(p0, SATURATING_CONVERGENCE_AMOUNT, COIN);
    const CTransaction tx1 = mk(p1, 2 * SATURATING_CONVERGENCE_AMOUNT, COIN);
    const CTransaction tx2 = mk(p2, 3 * SATURATING_CONVERGENCE_AMOUNT, COIN);

    CBlock b;
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << CScriptNum{1};
    cb.vout.emplace_back(0, CScript() << OP_RETURN);
    b.vtx.push_back(MakeTransactionRef(cb));
    b.vtx.push_back(MakeTransactionRef(tx0));
    b.vtx.push_back(MakeTransactionRef(tx1));
    b.vtx.push_back(MakeTransactionRef(tx2));

    const int h = 240;
    // Sigma-PoW-01: seed = the committing block's hash, payout height = h + 1.
    const uint256 bind = b.GetHash();
    const uint32_t payout_h = static_cast<uint32_t>(h) + 1;

    const uint256 convergence_root = Consensus::ComputeConvergenceRootFromConvergenceSet(b.vtx, coins, h, Params().GetConsensus());
    struct Candidate {
        arith_uint256 sigma;
        arith_uint256 weight;
        uint256 txid;
    };
    std::vector<Candidate> eligible;
    for (size_t i = 1; i < b.vtx.size(); ++i) {
        const CTransaction& tx = *b.vtx[i];
        BOOST_CHECK(ComputeConvergenceFee(tx, coins, h, Params().GetConsensus()) > 0);
        const arith_uint256 d = UintToArith256(SelectionDigest(tx.GetHash(), bind, payout_h, convergence_root));
        eligible.push_back({d, arith_uint256{static_cast<uint64_t>(Consensus::GetConvergenceAmount(tx))}, tx.GetHash()});
    }
    BOOST_REQUIRE_EQUAL(eligible.size(), 3U);
    // RFC-010 §1 Step 6: winner = argmin_i σ_i/f_i, compared without division as σ_a·f_b < σ_b·f_a.
    std::sort(eligible.begin(), eligible.end(), [&](const Candidate& a, const Candidate& b) {
        const Consensus::uint512 lhs = Consensus::rigor_math_detail::Mul256x256Full(a.sigma, b.weight);
        const Consensus::uint512 rhs = Consensus::rigor_math_detail::Mul256x256Full(b.sigma, a.weight);
        if (lhs < rhs) return true;
        if (rhs < lhs) return false;
        return ConvergenceWinnerTieBreakCompare(a.txid, b.txid, convergence_root) < 0;
    });

    const auto winner = GetBlockWinner(b, h, coins, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    BOOST_REQUIRE(winner.has_value());
    BOOST_CHECK(*winner == eligible.front().txid);
}

// SECURITY-001-A8 (Sigma-PoW-01). Before A-8 every σ input was chosen by the producer of the
// block that paid the draw: it could permute its own transaction set, watch the winner move,
// and stop once the winner was its own convergence — a re-roll costing one hash. A-8 moves the
// whole preimage into the parent, so a re-roll costs a proof-of-work solution. These two cases
// pin the property from both sides.
BOOST_AUTO_TEST_CASE(security001a8_draw_is_a_function_of_the_committing_block_alone)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint p0{uint256S("0x7700000000000000000000000000000000000000000000000000000000000001"), 0};
    const COutPoint p1{uint256S("0x7700000000000000000000000000000000000000000000000000000000000002"), 0};
    coins.AddCoin(p0, Coin(CTxOut{100002 * COIN, g_oru_spk}, 100, false), false);
    coins.AddCoin(p1, Coin(CTxOut{100002 * COIN, g_oru_spk}, 100, false), false);

    auto mk = [&](const COutPoint& prev) {
        CMutableTransaction m;
        m.vin.resize(1);
        m.vin[0].prevout = prev;
        m.vout.resize(2);
        m.vout[0].nValue = 100000 * COIN;
        m.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
        m.vout[1].nValue = COIN;
        m.vout[1].scriptPubKey = g_oru_spk;
        m.vout.push_back(MakeOwnerDeclarationOut());
        return CTransaction{m};
    };

    CBlock parent;
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << CScriptNum{1};
    cb.vout.emplace_back(0, CScript() << OP_RETURN);
    parent.vtx.push_back(MakeTransactionRef(cb));
    parent.vtx.push_back(MakeTransactionRef(mk(p0)));
    parent.vtx.push_back(MakeTransactionRef(mk(p1)));

    const int parent_h = 400;
    const ConvergenceDrawSet set = BuildConvergenceDrawSet(parent, parent_h, coins, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    BOOST_REQUIRE_EQUAL(set.candidates.size(), 2U);
    BOOST_CHECK_EQUAL(set.source_block_hash, parent.GetHash());
    BOOST_CHECK_EQUAL(set.source_height, parent_h);

    // Resolving takes nothing from the paying block — not its transactions, not its header, not
    // even its height. There is no argument left through which a producer at h could influence σ.
    const ConvergenceDrawOutcome a = ResolveConvergenceDraw(set, Consensus::RES_PROBABILITY_TARGET_CONST);
    const ConvergenceDrawOutcome b = ResolveConvergenceDraw(set, Consensus::RES_PROBABILITY_TARGET_CONST);
    BOOST_CHECK(a.sigmas == b.sigmas);
    BOOST_CHECK_EQUAL(a.winner_index, b.winner_index);

    // The σ preimage is exactly (txid, C, B_{h-1}, h-1+1).
    for (size_t i = 0; i < set.candidates.size(); ++i) {
        BOOST_CHECK_EQUAL(a.sigmas[i],
            SelectionDigest(set.candidates[i].txid, set.source_block_hash, parent_h + 1, set.convergence_root));
    }
}

BOOST_AUTO_TEST_CASE(security001a8_regrinding_sigma_requires_a_new_proof_of_work)
{
    // Same candidates, same C, same payout height; only the committing block's hash moves — which
    // on a real chain means a fresh nonce and a fresh proof-of-work solution.
    ConvergenceDrawSet set;
    set.source_height = 500;
    set.source_nbits = 0x207fffff;
    set.candidates.push_back(ConvergenceDrawCandidate{
        uint256S("0x0000000000000000000000000000000000000000000000000000000000000011"),
        static_cast<uint64_t>(100000 * COIN), std::vector<unsigned char>(32, 0xa1)});
    set.candidates.push_back(ConvergenceDrawCandidate{
        uint256S("0x0000000000000000000000000000000000000000000000000000000000000022"),
        static_cast<uint64_t>(100000 * COIN), std::vector<unsigned char>(32, 0xa2)});
    set.convergence_root = Consensus::ComputeConvergenceRootFromEligibilityTxids(
        {set.candidates[0].txid, set.candidates[1].txid});

    set.source_block_hash = uint256S("0x00000000000000000000000000000000000000000000000000000000000000aa");
    const ConvergenceDrawOutcome first = ResolveConvergenceDraw(set, Consensus::RES_PROBABILITY_TARGET_CONST);

    // A one-bit change in the seed reshuffles every σ.
    set.source_block_hash = uint256S("0x00000000000000000000000000000000000000000000000000000000000000ab");
    const ConvergenceDrawOutcome second = ResolveConvergenceDraw(set, Consensus::RES_PROBABILITY_TARGET_CONST);

    BOOST_REQUIRE_EQUAL(first.sigmas.size(), 2U);
    BOOST_REQUIRE_EQUAL(second.sigmas.size(), 2U);
    for (size_t i = 0; i < first.sigmas.size(); ++i) {
        BOOST_CHECK(first.sigmas[i] != second.sigmas[i]);
    }
}

BOOST_AUTO_TEST_CASE(convergence_winner_tiebreak_rfc000_direct_txid_memcmp)
{
    // RFC-000 §9: tie-break is unsigned byte-wise txid order; convergence_root is unused (API compat).
    uint256 tie_root;
    tie_root.SetHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    const uint256 txid_lo = uint256S("0x0000000000000000000000000000000000000000000000000000000000000001");
    const uint256 txid_hi = uint256S("0x0000000000000000000000000000000000000000000000000000000000000002");
    BOOST_CHECK(txid_lo < txid_hi);
    BOOST_CHECK(ConvergenceWinnerTieBreakCompare(txid_lo, txid_hi, tie_root) < 0);
    BOOST_CHECK(ConvergenceWinnerTieBreakCompare(txid_hi, txid_lo, tie_root) > 0);
}

BOOST_AUTO_TEST_CASE(convergence_winner_tiebreak_ignores_convergence_root)
{
    const uint256 t1 = uint256S("0x0000000000000000000000000000000000000000000000000000000000000001");
    const uint256 t2 = uint256S("0x0000000000000000000000000000000000000000000000000000000000000002");
    uint256 root_ff;
    root_ff.SetHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    const uint256 root_zero;
    const int c_ff = ConvergenceWinnerTieBreakCompare(t1, t2, root_ff);
    const int c_z = ConvergenceWinnerTieBreakCompare(t1, t2, root_zero);
    BOOST_CHECK_EQUAL(c_ff, c_z);
    BOOST_CHECK(c_ff < 0);
}

//BOOST_AUTO_TEST_CASE(strict_inequality_no_equal_case)
//{
//    const CAmount convergence_amt = COIN;
//    const arith_uint256 w = Consensus::ConvergenceAmountBetaWeight(convergence_amt);
//    BOOST_REQUIRE(w > arith_uint256{0});
//    const arith_uint256 t_exact{static_cast<uint64_t>(COIN)};
//    const uint32_t compact = t_exact.GetCompact();
//    bool neg{false}, over{false};
//    arith_uint256 t_roundtrip;
//    t_roundtrip.SetCompact(compact, &neg, &over);
//    BOOST_REQUIRE(!neg && !over);
//    BOOST_REQUIRE(t_roundtrip == t_exact);
//    const uint256 h_eq_u256 = ArithToUint256(w);
//    const arith_uint256 coin_u{static_cast<uint64_t>(COIN)};
//    const auto left = Consensus::rigor_math_detail::Mul256x256Full(UintToArith256(h_eq_u256), coin_u);
//    const auto right = Consensus::rigor_math_detail::Mul256x256Full(t_roundtrip, w);
//    BOOST_REQUIRE(left <= right);
//    BOOST_REQUIRE(!(left < right));
//    BOOST_CHECK(!Consensus::ConvergenceDrawInequalityHolds(h_eq_u256, compact, convergence_amt));
//}

//BOOST_AUTO_TEST_CASE(weight_saturates_consistently)
//{
//    const arith_uint256 max_u256 = ~arith_uint256{0};
//    BOOST_CHECK(Consensus::SafePow7(max_u256) == max_u256);
//
//    // For a large base, overflow must saturate deterministically.
//    const arith_uint256 large = arith_uint256{1} << 255;
//    const arith_uint256 sat_once = Consensus::SafePow7(large);
//    const arith_uint256 sat_twice = Consensus::SafePow7(large);
//    BOOST_CHECK(sat_once == max_u256);
//    BOOST_CHECK(sat_once == sat_twice);
//
//    // Root path determinism check on a moderate exact-power value (fast, no timeout).
//    const arith_uint256 x = arith_uint256{1} << 20; // 2^20 == 16^5 (exact)
//    const arith_uint256 r1 = Consensus::IntegerFifthRoot(x);
//    const arith_uint256 r2 = Consensus::IntegerFifthRoot(x);
//    BOOST_CHECK(r1 == r2);
//}

//BOOST_AUTO_TEST_CASE(rigor_hash_byte_exact)
//{
//    const uint256 txid = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
//    const uint256 bind = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
//    const uint64_t height = 123;
//    const uint32_t target_prev = 0x1d00ffff;
//    std::vector<unsigned char> preimage;
//    preimage.reserve(32 + 32 + 8 + 4);
//    preimage.insert(preimage.end(), txid.begin(), txid.end());
//    preimage.insert(preimage.end(), bind.begin(), bind.end());
//    unsigned char h_le[8];
//    unsigned char t_le[4];
//    WriteLE64(h_le, height);
//    WriteLE32(t_le, target_prev);
//    preimage.insert(preimage.end(), h_le, h_le + sizeof(h_le));
//    preimage.insert(preimage.end(), t_le, t_le + sizeof(t_le));
//    const uint256 convergence_root{};
//    preimage.insert(preimage.end(), convergence_root.begin(), convergence_root.end());
//    uint256 expect;
//    CHash256().Write(Span<const unsigned char>{preimage.data(), preimage.size()}).Finalize(expect);
//    BOOST_CHECK(expect == Consensus::ComputeRigorHash(txid, bind, height, target_prev, convergence_root));
//}

BOOST_AUTO_TEST_CASE(candidate_set_deterministic)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint p0{uint256S("0x0101010101010101010101010101010101010101010101010101010101010101"), 0};
    const COutPoint p1{uint256S("0x0202020202020202020202020202020202020202020202020202020202020202"), 0};
    coins.AddCoin(p0, Coin(CTxOut{12 * COIN, g_oru_spk}, 100, false), false);
    coins.AddCoin(p1, Coin(CTxOut{12 * COIN, g_oru_spk}, 100, false), false);

    auto mk = [&](const COutPoint& prev) {
        CMutableTransaction m;
        m.vin.resize(1);
        m.vin[0].prevout = prev;
        m.vout.resize(2);
        m.vout[0].nValue = COIN;
        m.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
        m.vout[1].nValue = 10 * COIN; // fee > 0
        m.vout[1].scriptPubKey = g_oru_spk;
        return CTransaction{m};
    };
    const CTransaction tx0 = mk(p0);
    const CTransaction tx1 = mk(p1);

    CBlock blk;
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << CScriptNum{1};
    cb.vout.emplace_back(0, CScript() << OP_RETURN);
    blk.vtx.push_back(MakeTransactionRef(cb));
    blk.vtx.push_back(MakeTransactionRef(tx0));
    blk.vtx.push_back(MakeTransactionRef(tx1));

    const int h = 300;

    // The live-view builder and the undo builder must agree candidate-for-candidate: since A-8
    // the undo path is what every node runs when it reads the parent's set off disk, so a
    // divergence here is a fork.
    const ConvergenceDrawSet set_view = BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), ConvergenceWindowTail::Empty());

    CBlockUndo undo;
    undo.vtxundo.resize(2);
    undo.vtxundo[0].vprevout.push_back(Coin(CTxOut{12 * COIN, g_oru_spk}, 100, false));
    undo.vtxundo[1].vprevout.push_back(Coin(CTxOut{12 * COIN, g_oru_spk}, 100, false));
    ConvergenceDrawSet set_undo;
    BOOST_REQUIRE(BuildConvergenceDrawSetFromUndo(blk, h, undo, Params().GetConsensus(), ConvergenceWindowTail::Empty(), set_undo));

    BOOST_CHECK_EQUAL(set_view.convergence_root, set_undo.convergence_root);
    BOOST_REQUIRE_EQUAL(set_view.candidates.size(), set_undo.candidates.size());
    for (size_t i = 0; i < set_view.candidates.size(); ++i) {
        BOOST_CHECK_EQUAL(set_view.candidates[i].txid, set_undo.candidates[i].txid);
        BOOST_CHECK_EQUAL(set_view.candidates[i].weight, set_undo.candidates[i].weight);
    }
}

BOOST_AUTO_TEST_CASE(min_convergence_boundary)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const COutPoint p{uint256S("0xabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabca"), 0};
    coins.AddCoin(p, Coin(CTxOut{2 * COIN, g_oru_spk}, 100, false), false);

    auto mk = [&](CAmount convergence_amt) {
        CMutableTransaction m;
        m.vin.resize(1);
        m.vin[0].prevout = p;
        m.vout.resize(2);
        m.vout[0].nValue = convergence_amt;
        m.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
        m.vout[1].nValue = 2 * COIN - convergence_amt - 1; // keep fee > 0
        m.vout[1].scriptPubKey = g_oru_spk;
        return CTransaction{m};
    };
    const CTransaction tx_below = mk(COIN - 1);
    const CTransaction tx_eq = mk(COIN);

    BOOST_CHECK(!IsConvergenceCandidate(tx_below));
    BOOST_CHECK(IsConvergenceCandidate(tx_eq));
}

// RFC-014 Conv-Decouple-01: this test was originally `target_prev_edge_cases`.
// Since the inequality RHS is now the constitutional constant
// `RES_PROBABILITY_TARGET`, `target_prev` no longer participates on the RHS.
// The test is rewritten to assert (a) zero-target rejects, (b) determinism
// across repeated calls under the new signature.
BOOST_AUTO_TEST_CASE(res_probability_target_edge_cases)
{
    const uint256 h = uint256S("0x0101010101010101010101010101010101010101010101010101010101010101");
    const CAmount convergence_amt = COIN;
    const uint256& res_probability_target = Consensus::RES_PROBABILITY_TARGET_CONST;
    const uint256 zero_target{};

    BOOST_CHECK(!Consensus::ConvergenceDrawInequalityHolds(h, zero_target, convergence_amt));
    const bool a = Consensus::ConvergenceDrawInequalityHolds(h, res_probability_target, convergence_amt);
    const bool b = Consensus::ConvergenceDrawInequalityHolds(h, res_probability_target, convergence_amt);
    BOOST_CHECK_EQUAL(a, b);
}

BOOST_AUTO_TEST_CASE(txid_vs_wtxid_locked)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint{uint256S("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), 0};
    mtx.vin[0].scriptWitness.stack.emplace_back(std::vector<unsigned char>{0x01, 0x02, 0x03});
    mtx.vout.resize(1);
    mtx.vout[0].nValue = COIN;
    mtx.vout[0].scriptPubKey = g_oru_spk;
    const CTransaction tx{mtx};

    const uint256 txid = tx.GetHash();
    const uint256 wtxid = tx.GetWitnessHash();
    BOOST_CHECK(txid != wtxid);

    const uint256 bind = uint256S("0x3333333333333333333333333333333333333333333333333333333333333333");
    const int h = 88;
    const uint256 convergence_root{};
    const uint256 hash_by_txid = SelectionDigest(txid, bind, h, convergence_root);
    const uint256 hash_by_wtxid = SelectionDigest(wtxid, bind, h, convergence_root);
    BOOST_CHECK(hash_by_txid != hash_by_wtxid);
}

BOOST_AUTO_TEST_SUITE_END()

// RFC-001 Golden Test Vectors - separate suite without fixture
BOOST_AUTO_TEST_SUITE(rfc001_test_vectors)

BOOST_AUTO_TEST_CASE(rfc001_golden_test_vectors)
{
    // CASE-002: single_tx_winner
    {
        std::vector<uint256> txids = {uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")};
        std::vector<CAmount> fees = {1};
        int height = 1;
        uint256 prev_hash = uint256S("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

        // Compute C (Convergence Root)
        uint256 C = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
        BOOST_CHECK_EQUAL(C.GetHex(), "0000000000000000000000000000000000000000000000000000000000000000"); // Single tx returns zero hash

        // Compute sigma for each txid (Sigma-PoW-01: prev_hash is the parent block hash itself)
        std::vector<uint256> sigma;
        for (const auto& txid : txids) {
            sigma.push_back(SelectionDigest(txid, prev_hash, height, C));
        }
        BOOST_CHECK_EQUAL(sigma[0].GetHex(), "1c6a409303ea1c6a07f4145f5888217ffbf570c97ebcfa3d694eabfe52b7f030");
        
        // RFC-015: a singleton convergence candidate may participate in selection.
        BOOST_CHECK_EQUAL(txids.size(), 1U);
    }
    
    // CASE-004: no_winner (RFC-014 reinterpretation)
    // Original case used `target_prev` corresponding to a very small target on the
    // RHS to force ineligibility. After RFC-014 the RHS is the constitutional
    // constant `RES_PROBABILITY_TARGET` (= floor(2^240 / MIN_CONVERGENCE_AMOUNT), see
    // SECURITY-001-A A-6). To preserve the "no_winner"
    // semantics under the new model we use fees = 0 (which makes `T·f = 0`,
    // i.e. inequality cannot hold for any non-zero σ). `target_prev` still
    // threads into σ for Sigma-CR-01 binding.
    {
        std::vector<uint256> txids = {
            uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
            uint256S("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
        };
        std::vector<CAmount> fees = {0, 0};
        int height = 5;
        uint256 prev_hash = uint256S("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
        const uint256& res_probability_target = Consensus::RES_PROBABILITY_TARGET_CONST;

        uint256 C = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
        BOOST_CHECK_EQUAL(C.GetHex(), "fb76b78e0fae95e9804262321cd913f27a1f41b799f3b0b7b93f37393b0d9d49");

        std::vector<bool> eligible;
        for (size_t i = 0; i < txids.size(); ++i) {
            uint256 h = SelectionDigest(txids[i], prev_hash, height, C);
            eligible.push_back(Consensus::ConvergenceDrawInequalityHolds(h, res_probability_target, fees[i]));
        }

        BOOST_CHECK_EQUAL(eligible[0], false);
        BOOST_CHECK_EQUAL(eligible[1], false);
    }
    
    // CASE-005: weight_priority
    {
        std::vector<uint256> txids = {
            uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
            uint256S("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
        };
        std::vector<CAmount> fees = {1, 1000};
        int height = 7;
        uint256 prev_hash = uint256S("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");

        // Compute C
        uint256 C = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
        BOOST_CHECK_EQUAL(C.GetHex(), "fb76b78e0fae95e9804262321cd913f27a1f41b799f3b0b7b93f37393b0d9d49");

        // Compute sigma for both
        uint256 h0 = SelectionDigest(txids[0], prev_hash, height, C);
        uint256 h1 = SelectionDigest(txids[1], prev_hash, height, C);
        
        // Winner should be index 1 (higher fee weight)
        // winner = argmin(H / f)
        // Cross-multiply: H0 * f1 < H1 * f0
        arith_uint256 H0 = UintToArith256(h0);
        arith_uint256 H1 = UintToArith256(h1);
        Consensus::uint512 lhs = Consensus::rigor_math_detail::Mul256x256Full(H0, arith_uint256(fees[1]));
        Consensus::uint512 rhs = Consensus::rigor_math_detail::Mul256x256Full(H1, arith_uint256(fees[0]));
        
        BOOST_CHECK(lhs.Compare(rhs) > 0); // txid[1] wins
    }
    
    // CASE-006: tie_break
    {
        std::vector<uint256> txids = {
            uint256S("1111111111111111111111111111111111111111111111111111111111111111"),
            uint256S("2222222222222222222222222222222222222222222222222222222222222222")
        };
        std::vector<CAmount> fees = {10, 10};
        int height = 100;
        uint256 prev_hash = uint256S("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");

        // Compute C
        uint256 C = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);

        // RFC-000 §9: the tie-break is unsigned byte-wise txid order, and it decides only when
        // the cross-multiplied comparison is exactly equal. Equal fees do NOT by themselves
        // produce a tie — they only reduce the comparison to σ against σ.
        BOOST_CHECK(std::memcmp(txids[0].begin(), txids[1].begin(), 32) < 0);
        BOOST_CHECK(ConvergenceWinnerTieBreakCompare(txids[0], txids[1], C) < 0);

        uint256 h0 = SelectionDigest(txids[0], prev_hash, height, C);
        uint256 h1 = SelectionDigest(txids[1], prev_hash, height, C);
        const Consensus::uint512 lhs =
            Consensus::rigor_math_detail::Mul256x256Full(UintToArith256(h0), arith_uint256(fees[1]));
        const Consensus::uint512 rhs =
            Consensus::rigor_math_detail::Mul256x256Full(UintToArith256(h1), arith_uint256(fees[0]));
        BOOST_CHECK_EQUAL(lhs.Compare(rhs) < 0, UintToArith256(h0) < UintToArith256(h1));
    }
    
    // CASE-007: big_int
    {
        std::vector<uint256> txids = {
            uint256::ONE,
            uint256S("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee")
        };
        std::vector<CAmount> fees = {CAmount(18446744073709551615ULL), 1}; // UINT64_MAX and 1
        int height = 100;
        uint256 prev_hash = uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

        // Compute C
        uint256 C = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
        BOOST_CHECK_EQUAL(C.GetHex(), "6feb3eb3f7475eea0bd7a757e8537c54af471ec3c48cbbd2188854445e419bfb");

        // Compute sigma for both
        uint256 h0 = SelectionDigest(txids[0], prev_hash, height, C);
        uint256 h1 = SelectionDigest(txids[1], prev_hash, height, C);
        
        // Test 512-bit arithmetic (no overflow)
        arith_uint256 H0 = UintToArith256(h0);
        arith_uint256 H1 = UintToArith256(h1);
        Consensus::uint512 lhs = Consensus::rigor_math_detail::Mul256x256Full(H0, arith_uint256(fees[1]));
        Consensus::uint512 rhs = Consensus::rigor_math_detail::Mul256x256Full(H1, arith_uint256(fees[0]));
        
        // Deterministic winner selection (txid[0] wins based on comparison)
        BOOST_CHECK(lhs.Compare(rhs) < 0); // txid[0] wins
    }
}

// =============================================================================
// SECURITY-001-A9-C: the candidate set spans a sliding window of blocks
//
// A-8 hardened the σ SEED (the draw is bound to an already-mined header) but left the
// candidate SET to the producer of block h-1, who chose it unilaterally and for free. With
// CONVERGENCE_DRAW_MIN_TXS_FOR_RES == 1 a single self-owned convergence was a valid draw, so a
// miner won every block he produced at the 1 ORU minimum and RES allocation tracked hashrate
// instead of burn — design review finding F-1.
//
// A-9-C widens F to [h-W, h-1]. What that buys is not a smaller share of F for a large miner
// (he still produces ≈ αW of the window) but the loss of EXCLUSIVITY: candidates in [h-W, h-2]
// are buried and cannot be censored, so he must outbid rather than exclude.
//
// `ConvergenceWindowTail` is a plain value precisely so these cases can drive arbitrary
// windows without a chainstate. The chain walk that fills it is exercised by the functional
// tests; what is pinned here is the set algebra every validator must reproduce byte-for-byte.
// =============================================================================
namespace {

/** A convergence candidate as it appears in a window tail, at a chosen weight and height. */
ConvergenceDrawCandidate MakeBuriedCandidate(unsigned char txid_fill, uint64_t weight, int source_height)
{
    ConvergenceDrawCandidate candidate;
    candidate.txid = uint256{std::vector<unsigned char>(32, txid_fill)};
    candidate.weight = weight;
    candidate.res_payout_program32 = std::vector<unsigned char>(32, 0xa1);
    candidate.source_height = source_height;
    return candidate;
}

/** A block carrying `count` valid draw candidates, with the coins they spend added to `coins`. */
CBlock MakeCandidateBlock(CCoinsViewCache& coins, unsigned char prevout_fill, int count)
{
    CBlock blk;
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << CScriptNum{1};
    cb.vout.emplace_back(0, CScript() << OP_RETURN);
    blk.vtx.push_back(MakeTransactionRef(cb));

    for (int i = 0; i < count; ++i) {
        std::vector<unsigned char> id(32, prevout_fill);
        id[0] = static_cast<unsigned char>(i + 1);
        const COutPoint prev{uint256{id}, 0};
        coins.AddCoin(prev, Coin(CTxOut{12 * COIN, g_oru_spk}, 100, false), false);

        CMutableTransaction m;
        m.vin.resize(1);
        m.vin[0].prevout = prev;
        m.vout.resize(3);
        m.vout[0].nValue = COIN; // convergence amount == MIN_CONVERGENCE_AMOUNT
        m.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
        m.vout[1] = MakeOwnerDeclarationOut(); // A-5 R1: without this it never enters F
        m.vout[2].nValue = 10 * COIN;          // leaves a positive fee
        m.vout[2].scriptPubKey = g_oru_spk;
        blk.vtx.push_back(MakeTransactionRef(CTransaction{m}));
    }
    return blk;
}

/** Undo data matching `MakeCandidateBlock`, so the two builders can be compared. */
CBlockUndo MakeCandidateBlockUndo(const CBlock& blk)
{
    CBlockUndo undo;
    undo.vtxundo.resize(blk.vtx.size() - 1);
    for (auto& tx_undo : undo.vtxundo) {
        tx_undo.vprevout.push_back(Coin(CTxOut{12 * COIN, g_oru_spk}, 100, false));
    }
    return undo;
}

} // namespace

BOOST_AUTO_TEST_CASE(a9c_buried_candidates_join_the_draw_set)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const CBlock blk = MakeCandidateBlock(coins, 0xd1, 1);
    const int h = 400;

    // The pre-A9C behaviour: F is the source block alone.
    const ConvergenceDrawSet narrow =
        BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    BOOST_REQUIRE_EQUAL(narrow.candidates.size(), 1U);

    // Two buried candidates from earlier blocks in the window must join F. This is the whole
    // point of A-9-C: the source block's producer cannot remove them.
    ConvergenceWindowTail tail = ConvergenceWindowTail::Empty();
    tail.candidates.push_back(MakeBuriedCandidate(0xb1, 5 * COIN, h - 2));
    tail.candidates.push_back(MakeBuriedCandidate(0xb2, 7 * COIN, h - 1));

    const ConvergenceDrawSet wide = BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), tail);
    BOOST_REQUIRE_EQUAL(wide.candidates.size(), 3U);

    // Oldest first, source block last: candidate indices must be a function of the window, not
    // of the order a node happened to learn the blocks in.
    BOOST_CHECK_EQUAL(wide.candidates[0].txid, tail.candidates[0].txid);
    BOOST_CHECK_EQUAL(wide.candidates[1].txid, tail.candidates[1].txid);
    BOOST_CHECK_EQUAL(wide.candidates[2].txid, narrow.candidates[0].txid);

    // Widening F necessarily moves C, which is a σ input, so the draw is a different draw.
    BOOST_CHECK(wide.convergence_root != narrow.convergence_root);

    // The source block still identifies the window: σ's seed and the payout height are unchanged
    // by A-9-C, which is what keeps A-8 intact.
    BOOST_CHECK_EQUAL(wide.source_block_hash, blk.GetHash());
    BOOST_CHECK_EQUAL(wide.source_height, h);
}

BOOST_AUTO_TEST_CASE(a9c_window_root_is_independent_of_candidate_order)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const CBlock blk = MakeCandidateBlock(coins, 0xd2, 1);
    const int h = 401;

    ConvergenceWindowTail forward = ConvergenceWindowTail::Empty();
    forward.candidates.push_back(MakeBuriedCandidate(0xb1, 5 * COIN, h - 3));
    forward.candidates.push_back(MakeBuriedCandidate(0xb2, 7 * COIN, h - 2));
    forward.candidates.push_back(MakeBuriedCandidate(0xb3, 9 * COIN, h - 1));

    ConvergenceWindowTail reversed = ConvergenceWindowTail::Empty();
    reversed.candidates.assign(forward.candidates.rbegin(), forward.candidates.rend());

    // C sorts its inputs lexicographically, so it commits to the SET and not to an enumeration
    // order. Two nodes that assemble the same window differently must still agree on σ.
    BOOST_CHECK_EQUAL(
        BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), forward).convergence_root,
        BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), reversed).convergence_root);
}

BOOST_AUTO_TEST_CASE(a9c_already_paid_candidate_leaves_the_draw_set)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const CBlock blk = MakeCandidateBlock(coins, 0xd3, 1);
    const int h = 402;

    const ConvergenceDrawCandidate buried = MakeBuriedCandidate(0xb1, 5 * COIN, h - 1);

    ConvergenceWindowTail tail = ConvergenceWindowTail::Empty();
    tail.candidates.push_back(buried);
    const ConvergenceDrawSet with_buried =
        BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), tail);
    BOOST_REQUIRE_EQUAL(with_buried.candidates.size(), 2U);

    // A candidate survives in the window for up to W draws, so a mint inside the window must
    // remove the txid it paid or the same convergence could be paid more than once.
    tail.already_paid.push_back(buried.txid);
    const ConvergenceDrawSet after_payout =
        BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), tail);
    BOOST_REQUIRE_EQUAL(after_payout.candidates.size(), 1U);
    BOOST_CHECK(after_payout.candidates[0].txid != buried.txid);

    // The exclusion must be indistinguishable from the candidate never having been there: C is a
    // σ input, so a node that merely skipped it in the winner loop would compute a different σ.
    const ConvergenceDrawSet without_buried =
        BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    BOOST_CHECK_EQUAL(after_payout.convergence_root, without_buried.convergence_root);
}

BOOST_AUTO_TEST_CASE(a9c_already_paid_also_excludes_a_source_block_candidate)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const CBlock blk = MakeCandidateBlock(coins, 0xd4, 2);
    const int h = 403;

    const ConvergenceDrawSet full =
        BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), ConvergenceWindowTail::Empty());
    BOOST_REQUIRE_EQUAL(full.candidates.size(), 2U);

    // The exclusion applies to the source block's own candidates too, not just buried ones: the
    // source block is inside the window it anchors.
    ConvergenceWindowTail tail = ConvergenceWindowTail::Empty();
    tail.already_paid.push_back(full.candidates[0].txid);

    const ConvergenceDrawSet pruned = BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), tail);
    BOOST_REQUIRE_EQUAL(pruned.candidates.size(), 1U);
    BOOST_CHECK_EQUAL(pruned.candidates[0].txid, full.candidates[1].txid);
    BOOST_CHECK(pruned.convergence_root != full.convergence_root);
}

BOOST_AUTO_TEST_CASE(a9c_incomplete_window_refuses_to_resolve)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const CBlock blk = MakeCandidateBlock(coins, 0xd5, 1);
    const int h = 404;

    // A window the chain layer could not read in full must surface as "no draw". Silently
    // resolving over a shortened window would give that node a different F, hence a different C
    // and a different winner — a fork whose cause is local disk state.
    ConvergenceWindowTail truncated;
    BOOST_REQUIRE(!truncated.complete);

    const ConvergenceDrawSet set = BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), truncated);
    BOOST_CHECK(!set.IsSet());
    BOOST_CHECK(set.candidates.empty());

    ConvergenceDrawSet from_undo;
    BOOST_CHECK(!BuildConvergenceDrawSetFromUndo(
        blk, h, MakeCandidateBlockUndo(blk), Params().GetConsensus(), truncated, from_undo));
}

BOOST_AUTO_TEST_CASE(a9c_view_and_undo_builders_agree_under_a_window)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const CBlock blk = MakeCandidateBlock(coins, 0xd6, 2);
    const int h = 405;

    ConvergenceWindowTail tail = ConvergenceWindowTail::Empty();
    tail.candidates.push_back(MakeBuriedCandidate(0xb1, 5 * COIN, h - 2));
    tail.candidates.push_back(MakeBuriedCandidate(0xb2, 7 * COIN, h - 1));
    tail.already_paid.push_back(MakeBuriedCandidate(0xb2, 7 * COIN, h - 1).txid);

    // The live-view builder runs during assembly and validation; the undo builder is what every
    // node runs when it reads a window off disk. A divergence between them is a fork, and the
    // window and its exclusions must not be where they start to differ.
    const ConvergenceDrawSet set_view = BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), tail);
    ConvergenceDrawSet set_undo;
    BOOST_REQUIRE(BuildConvergenceDrawSetFromUndo(
        blk, h, MakeCandidateBlockUndo(blk), Params().GetConsensus(), tail, set_undo));

    BOOST_CHECK_EQUAL(set_view.convergence_root, set_undo.convergence_root);
    BOOST_REQUIRE_EQUAL(set_view.candidates.size(), set_undo.candidates.size());
    BOOST_CHECK_EQUAL(set_view.candidates.size(), 3U); // 2 in-block + 2 buried - 1 already paid
    for (size_t i = 0; i < set_view.candidates.size(); ++i) {
        BOOST_CHECK_EQUAL(set_view.candidates[i].txid, set_undo.candidates[i].txid);
        BOOST_CHECK_EQUAL(set_view.candidates[i].weight, set_undo.candidates[i].weight);
    }

    // Same set, same seed, same height, therefore the same resolved draw on both paths.
    const ConvergenceDrawOutcome out_view =
        ResolveConvergenceDraw(set_view, Params().GetConsensus().RES_PROBABILITY_TARGET);
    const ConvergenceDrawOutcome out_undo =
        ResolveConvergenceDraw(set_undo, Params().GetConsensus().RES_PROBABILITY_TARGET);
    BOOST_CHECK_EQUAL(out_view.winner_index, out_undo.winner_index);
    BOOST_CHECK(out_view.sigmas == out_undo.sigmas);
}

BOOST_AUTO_TEST_CASE(a9c_decoy_proof_shaped_witness_does_not_mark_ticket_paid)
{
    const uint256 decoy_ticket = uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    const uint256 real_ticket = uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    const CAmount saturating =
        65536 * static_cast<CAmount>(Consensus::CONVERGENCE_DRAW_WINDOW) * Consensus::MIN_CONVERGENCE_AMOUNT;

    CBlock blk;
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << CScriptNum{1};
    cb.vout.emplace_back(0, CScript() << OP_RETURN);
    blk.vtx.push_back(MakeTransactionRef(cb));

    CMutableTransaction decoy;
    decoy.vin.resize(1);
    decoy.vin[0].prevout = COutPoint(uint256S("0x11"), 0);
    decoy.vout.emplace_back(COIN, g_oru_spk);
    decoy.vin[0].scriptWitness = BuildPcorbBodyResMintWitness(
        decoy_ticket, saturating, uint256::ONE, 200, 0x1d00ffff, uint256());
    blk.vtx.push_back(MakeTransactionRef(decoy));

    CMutableTransaction mint;
    mint.vin.resize(1);
    mint.vin[0].prevout = COutPoint(uint256S("0x22"), 0);
    mint.vout.emplace_back(Consensus::RES_REWARD_AMOUNT, MakeResPayoutScript(0xa1));
    mint.vin[0].scriptWitness = BuildPcorbBodyResMintWitness(
        real_ticket, saturating, uint256::ONE, 200, 0x1d00ffff, uint256());
    blk.vtx.push_back(MakeTransactionRef(mint));

    const std::vector<uint256> paid = FindBlockPaidConvergenceTxids(blk);
    BOOST_REQUIRE_EQUAL(paid.size(), 1U);
    BOOST_CHECK_EQUAL(paid[0], real_ticket);
}

BOOST_AUTO_TEST_CASE(a9c_mint_excludes_named_ticket_identically_on_view_and_undo)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const uint256 buried_ticket = uint256S("0xabababababababababababababababababababababababababababababababab");
    const CAmount saturating =
        65536 * static_cast<CAmount>(Consensus::CONVERGENCE_DRAW_WINDOW) * Consensus::MIN_CONVERGENCE_AMOUNT;

    CBlock blk = MakeCandidateBlock(coins, 0xd7, 1);
    const COutPoint mint_prev{uint256S("0xe2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2e2"), 0};
    coins.AddCoin(mint_prev, Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);

    CMutableTransaction mint;
    mint.vin.resize(1);
    mint.vin[0].prevout = mint_prev;
    mint.vout.emplace_back(Consensus::RES_REWARD_AMOUNT, MakeResPayoutScript(0xa1));
    mint.vin[0].scriptWitness = BuildPcorbBodyResMintWitness(
        buried_ticket, saturating, uint256::ONE, 200, 0x1d00ffff, uint256());
    blk.vtx.push_back(MakeTransactionRef(mint));

    CBlockUndo undo = MakeCandidateBlockUndo(blk);
    // MakeCandidateBlockUndo assumes every body tx spent 12 ORU; the mint spent 20.
    undo.vtxundo.back().vprevout[0] = Coin(CTxOut(20 * COIN, g_oru_spk), 100, false);

    ConvergenceWindowTail tail = ConvergenceWindowTail::Empty();
    tail.candidates.push_back(MakeBuriedCandidate(0xab, COIN, 10));
    BOOST_REQUIRE_EQUAL(tail.candidates.back().txid, buried_ticket);

    const int h = 11;
    const ConvergenceDrawSet set_view = BuildConvergenceDrawSet(blk, h, coins, Params().GetConsensus(), tail);
    ConvergenceDrawSet set_undo;
    BOOST_REQUIRE(BuildConvergenceDrawSetFromUndo(blk, h, undo, Params().GetConsensus(), tail, set_undo));

    BOOST_CHECK(FindBlockPaidConvergenceTxids(blk) == std::vector<uint256>{buried_ticket});
    BOOST_REQUIRE_EQUAL(set_view.candidates.size(), 1U);
    BOOST_REQUIRE_EQUAL(set_undo.candidates.size(), 1U);
    BOOST_CHECK_EQUAL(set_view.candidates[0].txid, blk.vtx[1]->GetHash());
    BOOST_CHECK_EQUAL(set_undo.candidates[0].txid, blk.vtx[1]->GetHash());
    BOOST_CHECK(set_view.candidates[0].txid != buried_ticket);
    BOOST_CHECK_EQUAL(set_view.convergence_root, set_undo.convergence_root);
}

BOOST_AUTO_TEST_CASE(a9c_reorg_branch_without_mint_does_not_inherit_already_paid)
{
    const uint256 ticket = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    const CAmount saturating =
        65536 * static_cast<CAmount>(Consensus::CONVERGENCE_DRAW_WINDOW) * Consensus::MIN_CONVERGENCE_AMOUNT;

    CBlock with_mint;
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vin[0].scriptSig = CScript() << CScriptNum{1};
    cb.vout.emplace_back(0, CScript() << OP_RETURN);
    with_mint.vtx.push_back(MakeTransactionRef(cb));

    CMutableTransaction mint;
    mint.vin.resize(1);
    mint.vin[0].prevout = COutPoint(uint256::ONE, 0);
    mint.vout.emplace_back(Consensus::RES_REWARD_AMOUNT, MakeResPayoutScript(0xa1));
    mint.vin[0].scriptWitness = BuildPcorbBodyResMintWitness(
        ticket, saturating, uint256::ONE, 200, 0x1d00ffff, uint256());
    with_mint.vtx.push_back(MakeTransactionRef(mint));

    CBlock sibling = with_mint;
    sibling.vtx.resize(1);

    BOOST_CHECK_EQUAL(FindBlockPaidConvergenceTxids(with_mint).size(), 1U);
    BOOST_CHECK(FindBlockPaidConvergenceTxids(sibling).empty());
}

BOOST_AUTO_TEST_SUITE_END()
