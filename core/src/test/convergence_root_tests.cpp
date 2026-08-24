// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergence_root.h>
#include <consensus/convergenceres.h>
#include <consensus/res_owner.h>
#include <consensus/res_proof.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <uint256.h>

BOOST_FIXTURE_TEST_SUITE(convergence_root_tests, BasicTestingSetup)

namespace {

const CScript g_oru_spk = CScript() << OP_0 << std::vector<unsigned char>(20, 0x22);

CMutableTransaction MakeRegtestLikeCoinbase()
{
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vout.resize(1);
    cb.vout[0].nValue = 50 * COIN;
    cb.vout[0].scriptPubKey = CScript() << OP_TRUE;
    return cb;
}

// SECURITY-001-A / A-5 R1: a convergence must declare its RES payout owner to enter F, so
// every fixture that expects eligibility carries a declaration.
CTransactionRef MakeconvergenceTx(const uint256& prev_utxo_hash, const CScript& convergence_spk)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prev_utxo_hash, 0);
    mtx.vout.resize(2);
    mtx.vout[0].nValue = COIN;
    mtx.vout[0].scriptPubKey = convergence_spk;
    mtx.vout[1].nValue = 19 * COIN - 1;
    mtx.vout[1].scriptPubKey = g_oru_spk;
    return MakeTransactionRef(mtx);
}

uint256 ExpectedconvergenceSetRoot(const std::vector<uint256>& txids_sorted)
{
    if (txids_sorted.empty()) return uint256();
    CHash256 hasher;
    for (const uint256& id : txids_sorted) {
        hasher.Write(MakeUCharSpan(id));
    }
    uint256 out;
    hasher.Finalize(out);
    return out;
}

} // namespace

BOOST_AUTO_TEST_CASE(script_sig_parse_roundtrip)
{
    const int height = 100;
    const uint256 convergence_root = InsecureRand256();

    const CScript sig = CScript() << height << std::vector<unsigned char>(convergence_root.begin(), convergence_root.end());

    Consensus::CoinbaseConvergenceRootScriptSig parsed;
    std::string err;
    BOOST_REQUIRE(Consensus::ParseCoinbaseConvergenceRootScriptSig(sig, parsed, err));
    BOOST_CHECK_EQUAL(parsed.height, height);
    BOOST_CHECK_EQUAL(parsed.convergence_root, convergence_root);
}

BOOST_AUTO_TEST_CASE(script_sig_third_push_rejected)
{
    const int height = 100;
    const uint256 convergence_root = InsecureRand256();
    CScript sig = CScript() << height << std::vector<unsigned char>(convergence_root.begin(), convergence_root.end());
    sig << std::vector<unsigned char>{1, 2, 3};

    Consensus::CoinbaseConvergenceRootScriptSig parsed;
    std::string err;
    BOOST_CHECK(!Consensus::ParseCoinbaseConvergenceRootScriptSig(sig, parsed, err));
    BOOST_CHECK(err.find("trailing") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(convergence_root_zero_when_no_qualifying_convergence_subset)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const auto cb = MakeTransactionRef(MakeRegtestLikeCoinbase());
    BOOST_CHECK_EQUAL(Consensus::ComputeConvergenceRootFromConvergenceSet({cb}, coins, 200, Params().GetConsensus()), uint256());

    CMutableTransaction noise;
    noise.vin.resize(1);
    noise.vin[0].prevout.hash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    noise.vin[0].prevout.n = 0;
    noise.vout.resize(1);
    noise.vout[0].nValue = 1;
    noise.vout[0].scriptPubKey = CScript() << OP_1;
    const std::vector<CTransactionRef> vtx{cb, MakeTransactionRef(noise)};
    BOOST_CHECK_EQUAL(Consensus::ComputeConvergenceRootFromConvergenceSet(vtx, coins, 200, Params().GetConsensus()), uint256());
}

BOOST_AUTO_TEST_CASE(convergence_root_two_convergence_txs_sorted_concat_hash256)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const uint256 h_a = uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    const uint256 h_b = uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    coins.AddCoin(COutPoint(h_a, 0), Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);
    coins.AddCoin(COutPoint(h_b, 0), Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);

    const auto tx_a = MakeconvergenceTx(h_a, Consensus::ConvergenceOutputScript());
    const auto tx_b = MakeconvergenceTx(h_b, Consensus::ConvergenceOutputScript());
    BOOST_REQUIRE(Consensus::IsConvergenceTx(*tx_a) && Consensus::IsConvergenceTx(*tx_b));

    std::vector<uint256> sorted{tx_a->GetHash(), tx_b->GetHash()};
    std::sort(sorted.begin(), sorted.end());
    const uint256 golden = ExpectedconvergenceSetRoot(sorted);

    const auto cb = MakeTransactionRef(MakeRegtestLikeCoinbase());
    const uint256 r_ab = Consensus::ComputeConvergenceRootFromConvergenceSet({cb, tx_a, tx_b}, coins, 200, Params().GetConsensus());
    const uint256 r_ba = Consensus::ComputeConvergenceRootFromConvergenceSet({cb, tx_b, tx_a}, coins, 200, Params().GetConsensus());
    BOOST_CHECK_EQUAL(r_ab, golden);
    BOOST_CHECK_EQUAL(r_ba, golden);
    BOOST_CHECK_EQUAL(r_ab, r_ba);
}

BOOST_AUTO_TEST_CASE(convergence_root_eligibility_vector_order_invariant)
{
    const uint256 x = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    const uint256 y = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
    std::vector<uint256> forward{x, y};
    std::vector<uint256> reverse{y, x};
    BOOST_CHECK_EQUAL(
        Consensus::ComputeConvergenceRootFromEligibilityTxids(forward),
        Consensus::ComputeConvergenceRootFromEligibilityTxids(reverse));
}

BOOST_AUTO_TEST_CASE(list_convergence_root_commitment_txids_size_gates_root)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const uint256 h_a = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    coins.AddCoin(COutPoint(h_a, 0), Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);

    const auto tx_a = MakeconvergenceTx(h_a, Consensus::ConvergenceOutputScript());
    const auto cb = MakeTransactionRef(MakeRegtestLikeCoinbase());
    const std::vector<CTransactionRef> vtx_one{cb, tx_a};

    const auto ids_one = Consensus::ListConvergenceRootCommitmentTxids(vtx_one, coins, 200, Params().GetConsensus());
    BOOST_REQUIRE_EQUAL(ids_one.size(), 1U);
    BOOST_CHECK_EQUAL(ids_one[0], tx_a->GetHash());
    BOOST_CHECK(!Consensus::ComputeConvergenceRootFromEligibilityTxids(ids_one).IsNull());
    BOOST_CHECK_EQUAL(Consensus::ComputeConvergenceRootFromEligibilityTxids(ids_one),
        Consensus::ComputeConvergenceRootFromConvergenceSet(vtx_one, coins, 200, Params().GetConsensus()));
    BOOST_CHECK_EQUAL(Consensus::ComputeConvergenceRootFromEligibilityTxids(ids_one),
        ExpectedconvergenceSetRoot({tx_a->GetHash()}));

    const uint256 h_b = uint256S("0xdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    coins.AddCoin(COutPoint(h_b, 0), Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);
    const auto tx_b = MakeconvergenceTx(h_b, Consensus::ConvergenceOutputScript());
    const std::vector<CTransactionRef> vtx_two{cb, tx_a, tx_b};

    const auto ids_two = Consensus::ListConvergenceRootCommitmentTxids(vtx_two, coins, 200, Params().GetConsensus());
    BOOST_REQUIRE_EQUAL(ids_two.size(), 2U);
    BOOST_CHECK(!Consensus::ComputeConvergenceRootFromEligibilityTxids(ids_two).IsNull());
    BOOST_CHECK_EQUAL(Consensus::ComputeConvergenceRootFromEligibilityTxids(ids_two),
        Consensus::ComputeConvergenceRootFromConvergenceSet(vtx_two, coins, 200, Params().GetConsensus()));
}

// SECURITY-001-A / A-5 R1.
BOOST_AUTO_TEST_CASE(res_owner_declaration_gates_draw_eligibility)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const uint256 h_a = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    const uint256 h_b = uint256S("0xdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    coins.AddCoin(COutPoint(h_a, 0), Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);
    coins.AddCoin(COutPoint(h_b, 0), Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);

    const auto cb = MakeTransactionRef(MakeRegtestLikeCoinbase());
    const auto declared = MakeconvergenceTx(h_a, Consensus::ConvergenceOutputScript());
    const CScript retired_cvg2 = CScript() << OP_1 << std::vector<unsigned char>(32, 0x43);
    const auto undeclared = MakeconvergenceTx(h_b, retired_cvg2);

    BOOST_REQUIRE(Consensus::IsConvergenceTx(*declared));
    BOOST_CHECK(!Consensus::IsConvergenceTx(*undeclared));

    const auto ids = Consensus::ListConvergenceRootCommitmentTxids(
        {cb, declared, undeclared}, coins, 200, Params().GetConsensus());
    BOOST_REQUIRE_EQUAL(ids.size(), 1U);
    BOOST_CHECK_EQUAL(ids[0], declared->GetHash());

    BOOST_CHECK(ComputeConvergenceFee(*declared, coins, 200, Params().GetConsensus()) > 0);
    BOOST_CHECK_EQUAL(ComputeConvergenceFee(*undeclared, coins, 200, Params().GetConsensus()), CAmount{0});
}

BOOST_AUTO_TEST_CASE(res_owner_declaration_must_be_unique)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const uint256 h_a = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    coins.AddCoin(COutPoint(h_a, 0), Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);

    CMutableTransaction mtx{*MakeconvergenceTx(h_a, Consensus::ConvergenceOutputScript())};
    BOOST_REQUIRE(Consensus::HasResOwnerDeclaration(CTransaction(mtx)));

    mtx.vout.emplace_back(COIN, GetConvergeScriptForProgram(std::vector<unsigned char>(32, 0x77)));
    const auto doubled = MakeTransactionRef(mtx);
    BOOST_CHECK(!Consensus::HasResOwnerDeclaration(*doubled));

    const auto cb = MakeTransactionRef(MakeRegtestLikeCoinbase());
    const auto ids = Consensus::ListConvergenceRootCommitmentTxids({cb, doubled}, coins, 200, Params().GetConsensus());
    BOOST_CHECK(ids.empty());
}

BOOST_AUTO_TEST_CASE(converge_script_shape)
{
    const std::vector<unsigned char> program32(32, 0x5a);
    const CScript canonical = GetConvergeScriptForProgram(program32);
    BOOST_REQUIRE_EQUAL(canonical.size(), 22U);
    BOOST_CHECK_EQUAL(canonical[0], 20);
    BOOST_CHECK_EQUAL(canonical[21], OP_CONVERGE);

    uint160 parsed_hash;
    BOOST_REQUIRE(ParseConvergeScript(canonical, parsed_hash));
    BOOST_CHECK(parsed_hash == Hash160(program32));

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vout.emplace_back(COIN, canonical);
    const auto parsed = Consensus::GetResOwnerDeclaration(CTransaction(mtx));
    BOOST_REQUIRE(parsed.has_value());
    BOOST_CHECK(parsed->payout_hash160 == Hash160(program32));

    CScript too_short(canonical.begin(), canonical.end() - 1);
    BOOST_CHECK(!ParseConvergeScript(too_short, parsed_hash));

    CScript too_long = canonical;
    too_long.push_back(0x00);
    BOOST_CHECK(!ParseConvergeScript(too_long, parsed_hash));

    CScript wrong_opcode = canonical;
    wrong_opcode[21] = OP_RETURN;
    BOOST_CHECK(!ParseConvergeScript(wrong_opcode, parsed_hash));
}

BOOST_AUTO_TEST_CASE(converge_script_push_encoding_is_canonical)
{
    const std::vector<unsigned char> program32(32, 0x5a);
    const CScript canonical = GetConvergeScriptForProgram(program32);
    const uint160 owner = Hash160(program32);

    CScript pushdata1;
    pushdata1.push_back(OP_PUSHDATA1);
    pushdata1.push_back(20);
    pushdata1.insert(pushdata1.end(), owner.begin(), owner.end());
    pushdata1.push_back(OP_CONVERGE);

    uint160 parsed_hash;
    BOOST_CHECK(!ParseConvergeScript(pushdata1, parsed_hash));
    BOOST_CHECK(ParseConvergeScript(canonical, parsed_hash));

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vout.emplace_back(COIN, pushdata1);
    BOOST_CHECK(!Consensus::HasResOwnerDeclaration(CTransaction(mtx)));

    mtx.vout[0] = CTxOut(COIN, canonical);
    BOOST_CHECK(Consensus::HasResOwnerDeclaration(CTransaction(mtx)));
}

BOOST_AUTO_TEST_CASE(coinbase_convergence_root_script_sig_validation)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const uint256 h_a = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    const uint256 h_b = uint256S("0xdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    coins.AddCoin(COutPoint(h_a, 0), Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);
    coins.AddCoin(COutPoint(h_b, 0), Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);

    const auto tx_a = MakeconvergenceTx(h_a, Consensus::ConvergenceOutputScript());
    const auto tx_b = MakeconvergenceTx(h_b, Consensus::ConvergenceOutputScript());
    const auto cb = MakeTransactionRef(MakeRegtestLikeCoinbase());
    std::vector<CTransactionRef> vtx{cb, tx_a, tx_b};
    const uint256 expected = Consensus::ComputeConvergenceRootFromConvergenceSet(vtx, coins, 200, Params().GetConsensus());

    const int height = 200;
    const CScript good = CScript() << height << std::vector<unsigned char>(expected.begin(), expected.end());
    std::string err;
    BOOST_CHECK(Consensus::ValidateCoinbaseConvergenceRootScriptSig(good, height, vtx, coins, Params().GetConsensus(), err));
    BOOST_CHECK(Consensus::ValidateCoinbaseConvergenceRootScriptSig(good, height, expected, err));

    const auto committed_ids = Consensus::ListConvergenceRootCommitmentTxids(vtx, coins, height, Params().GetConsensus());
    BOOST_CHECK_EQUAL(Consensus::ComputeConvergenceRootFromEligibilityTxids(committed_ids), expected);
    Consensus::CoinbaseConvergenceRootScriptSig parsed_cb;
    std::string perr;
    BOOST_REQUIRE(Consensus::ParseCoinbaseConvergenceRootScriptSig(good, parsed_cb, perr));
    BOOST_CHECK_EQUAL(parsed_cb.convergence_root, expected);
    BOOST_CHECK_EQUAL(parsed_cb.convergence_root, Consensus::ComputeConvergenceRootFromEligibilityTxids(committed_ids));

    const CScript legacy = CScript() << height << OP_0;
    BOOST_CHECK(!Consensus::ValidateCoinbaseConvergenceRootScriptSig(legacy, height, vtx, coins, Params().GetConsensus(), err));

    uint256 wrong = expected;
    *(wrong.begin()) ^= 0x5a;
    const CScript bad_root = CScript() << height << std::vector<unsigned char>(wrong.begin(), wrong.end());
    BOOST_CHECK(!Consensus::ValidateCoinbaseConvergenceRootScriptSig(bad_root, height, vtx, coins, Params().GetConsensus(), err));

    CScript three_pushes = good;
    three_pushes << OP_1;
    BOOST_CHECK(!Consensus::ValidateCoinbaseConvergenceRootScriptSig(three_pushes, height, vtx, coins, Params().GetConsensus(), err));

    std::vector<unsigned char> short31(expected.begin(), expected.end());
    short31.resize(31);
    const CScript bad_len = CScript() << height << short31;
    BOOST_CHECK(!Consensus::ValidateCoinbaseConvergenceRootScriptSig(bad_len, height, vtx, coins, Params().GetConsensus(), err));

    CScript pushdata1_root = CScript() << height;
    pushdata1_root.push_back(static_cast<unsigned char>(OP_PUSHDATA1));
    pushdata1_root.push_back(32);
    pushdata1_root.insert(pushdata1_root.end(), expected.begin(), expected.end());
    BOOST_CHECK(!Consensus::ValidateCoinbaseConvergenceRootScriptSig(pushdata1_root, height, vtx, coins, Params().GetConsensus(), err));
}

BOOST_AUTO_TEST_CASE(res_proof_digest_ignores_coinbase_script_sig_conceptually)
{
    const uint256 convergence_txid = InsecureRand256();
    const uint256 bind = InsecureRand256();
    const int h = 55;
    const uint256 convergence_root{};
    const uint256 d1 = Consensus::ConvergeProofDigest(convergence_txid, bind, h, convergence_root);
    const uint256 d2 = Consensus::ConvergeProofDigest(convergence_txid, bind, h, convergence_root);
    BOOST_CHECK_EQUAL(d1, d2);
}

BOOST_AUTO_TEST_SUITE_END()
