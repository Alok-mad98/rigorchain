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
CTransactionRef MakeconvergenceTx(const uint256& prev_utxo_hash, const CScript& convergence_spk, bool declare_owner = true)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = COutPoint(prev_utxo_hash, 0);
    mtx.vout.resize(2);
    mtx.vout[0].nValue = COIN;
    mtx.vout[0].scriptPubKey = convergence_spk;
    mtx.vout[1].nValue = 19 * COIN - 1;
    mtx.vout[1].scriptPubKey = g_oru_spk;
    if (declare_owner) {
        // Derive a distinct owner program per convergence so fixtures stay independent.
        std::vector<unsigned char> program32(prev_utxo_hash.begin(), prev_utxo_hash.end());
        mtx.vout.emplace_back(0, Consensus::ResOwnerDeclarationScript(program32));
    }
    return MakeTransactionRef(mtx);
}

uint256 ExpectedconvergenceSetRoot(const std::vector<uint256>& txids_sorted)
{
    if (txids_sorted.size() < 2) return uint256();
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
    BOOST_CHECK(Consensus::ComputeConvergenceRootFromEligibilityTxids(ids_one).IsNull());
    BOOST_CHECK(Consensus::ComputeConvergenceRootFromConvergenceSet(vtx_one, coins, 200, Params().GetConsensus()).IsNull());

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
    const auto declared = MakeconvergenceTx(h_a, Consensus::ConvergenceOutputScript(), /*declare_owner=*/true);
    const auto undeclared = MakeconvergenceTx(h_b, Consensus::ConvergenceOutputScript(), /*declare_owner=*/false);

    // Both are structurally convergences and both burn ORU; only the declared one may play.
    BOOST_REQUIRE(Consensus::IsConvergenceTx(*declared));
    BOOST_REQUIRE(Consensus::IsConvergenceTx(*undeclared));

    const auto ids = Consensus::ListConvergenceRootCommitmentTxids(
        {cb, declared, undeclared}, coins, 200, Params().GetConsensus());
    BOOST_REQUIRE_EQUAL(ids.size(), 1U);
    BOOST_CHECK_EQUAL(ids[0], declared->GetHash());

    // The undo/reorg path must reach the same verdict or a reorg would rebuild a different F.
    BOOST_CHECK(ComputeConvergenceFee(*declared, coins, 200, Params().GetConsensus()) > 0);
    BOOST_CHECK_EQUAL(ComputeConvergenceFee(*undeclared, coins, 200, Params().GetConsensus()), CAmount{0});
}

// A second declaration is an ambiguity the block producer would get to resolve.
BOOST_AUTO_TEST_CASE(res_owner_declaration_must_be_unique)
{
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const uint256 h_a = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    coins.AddCoin(COutPoint(h_a, 0), Coin(CTxOut(20 * COIN, g_oru_spk), 100, false), false);

    CMutableTransaction mtx{*MakeconvergenceTx(h_a, Consensus::ConvergenceOutputScript())};
    BOOST_REQUIRE(Consensus::HasResOwnerDeclaration(CTransaction(mtx)));

    mtx.vout.emplace_back(0, Consensus::ResOwnerDeclarationScript(std::vector<unsigned char>(32, 0x77)));
    const auto doubled = MakeTransactionRef(mtx);
    BOOST_CHECK(!Consensus::HasResOwnerDeclaration(*doubled));

    const auto cb = MakeTransactionRef(MakeRegtestLikeCoinbase());
    const auto ids = Consensus::ListConvergenceRootCommitmentTxids({cb, doubled}, coins, 200, Params().GetConsensus());
    BOOST_CHECK(ids.empty());
}

// Byte-shape parser: exact size, exact magic, exact version, single encoding.
BOOST_AUTO_TEST_CASE(res_owner_declaration_payload_shape)
{
    const std::vector<unsigned char> program32(32, 0x5a);
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vout.emplace_back(0, Consensus::ResOwnerDeclarationScript(program32));
    const auto parsed = Consensus::GetResOwnerDeclaration(CTransaction(mtx));
    BOOST_REQUIRE(parsed.has_value());
    BOOST_CHECK(parsed->program32 == program32);

    std::vector<unsigned char> payload(Consensus::RES_OWNER_PAYLOAD_SIZE, 0x00);
    payload[0] = 'R'; payload[1] = 'E'; payload[2] = 'S'; payload[3] = 'O';
    payload[4] = Consensus::RES_OWNER_VERSION;
    BOOST_CHECK(Consensus::ParseResOwnerDeclarationPayload(payload).has_value());

    auto wrong_version = payload;
    wrong_version[4] = 0x02;
    BOOST_CHECK(!Consensus::ParseResOwnerDeclarationPayload(wrong_version).has_value());

    auto wrong_magic = payload;
    wrong_magic[0] = 'X';
    BOOST_CHECK(!Consensus::ParseResOwnerDeclarationPayload(wrong_magic).has_value());

    auto too_long = payload;
    too_long.push_back(0x00);
    BOOST_CHECK(!Consensus::ParseResOwnerDeclarationPayload(too_long).has_value());

    auto too_short = payload;
    too_short.pop_back();
    BOOST_CHECK(!Consensus::ParseResOwnerDeclarationPayload(too_short).has_value());
}

// One declaration must have one spelling: the payload pushed with OP_PUSHDATA1 carries the
// same bytes but is not the canonical script, so it does not count as a declaration.
BOOST_AUTO_TEST_CASE(res_owner_declaration_push_encoding_is_canonical)
{
    const std::vector<unsigned char> program32(32, 0x5a);
    const CScript canonical = Consensus::ResOwnerDeclarationScript(program32);
    BOOST_REQUIRE_EQUAL(canonical.size(), 2U + Consensus::RES_OWNER_PAYLOAD_SIZE);
    BOOST_CHECK_EQUAL(canonical[0], OP_RETURN);
    BOOST_CHECK_EQUAL(canonical[1], static_cast<unsigned char>(Consensus::RES_OWNER_PAYLOAD_SIZE));

    std::vector<unsigned char> payload(canonical.begin() + 2, canonical.end());
    CScript pushdata1;
    pushdata1 << OP_RETURN;
    pushdata1.push_back(OP_PUSHDATA1);
    pushdata1.push_back(static_cast<unsigned char>(payload.size()));
    pushdata1.insert(pushdata1.end(), payload.begin(), payload.end());

    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vout.emplace_back(0, pushdata1);
    BOOST_CHECK(!Consensus::HasResOwnerDeclaration(CTransaction(mtx)));

    mtx.vout[0] = CTxOut(0, canonical);
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
