// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/convergence_root.h>
#include <consensus/rigor_math.h>
#include <consensus/convergenceres.h>
#include <consensus/res_amount.h>
#include <test/util/pcorb_carrier.h>
#include <consensus/res_owner.h>
#include <consensus/res_proof.h>
#include <consensus/rigor_res.h>
#include <consensus/validation.h>
#include <crypto/common.h>
#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/descriptor.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <span.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <algorithm>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

#include <boost/test/unit_test.hpp>

namespace {

/**
 * Weight large enough that `σ < T·f` holds unconditionally.
 *
 * After SECURITY-001-A finding A-6 the eligibility probability is `p(n) = n / C` for a candidate
 * converging `n` ORU, with `C = 2^(256-E)`. SECURITY-001-A9-C then divided the target by the
 * window W = 32, so the PER-DRAW probability is `p(n) = n / (C·W)`. At the RFC-017 exponent
 * E = 246 that is `C = 1024` and per-draw saturation at 32 768 ORU. This constant sits far above
 * that bound and also above the bound at the previous E = 240 (2 097 152 ORU).
 *
 * Tests whose subject is signature-domain separation rather than draw odds use this weight so
 * they do not incidentally depend on the value of RES_PROBABILITY_TARGET.
 */
constexpr CAmount SATURATING_CONVERGENCE_AMOUNT = 4000000 * COIN;
static_assert(SATURATING_CONVERGENCE_AMOUNT <= MAX_MONEY, "saturating test weight must be a representable amount");

// RFC-014 Conv-Decouple-01: the RES inequality RHS is the constitutional constant
// `Consensus::Params::RES_PROBABILITY_TARGET` (= floor(2^246 / MIN_CONVERGENCE_AMOUNT), see
// SECURITY-001-A A-6), accessed below via the active chain params provided by
// `BasicTestingSetup`.
//
// SECURITY-001-A8: `bind` is now the parent block hash and `target_prev` is no longer part of
// the σ preimage, so the search space here is (txid, C, bind, height).
bool FindPassingConvergenceTxid(uint256& convergence_out, const uint256& bind, uint32_t height, CAmount convergence_amt, const uint256& convergence_root = {})
{
    const uint256& res_probability_target = Params().GetConsensus().RES_PROBABILITY_TARGET;
    for (uint64_t n = 1; n < 50000000; ++n) {
        convergence_out = uint256S(strprintf("0x%064x", static_cast<unsigned>(n)));
        const uint256 h = Consensus::ConvergeProofDigest(convergence_out, bind, height, convergence_root);
        if (Consensus::ConvergenceDrawInequalityHolds(h, res_probability_target, convergence_amt)) {
            return true;
        }
    }
    return false;
}

void FillProofContext(Consensus::ConvergeProof& proof,
    const uint256& convergence_txid,
    CAmount convergence_amt,
    const uint256& bind,
    uint64_t height,
    uint32_t target_prev,
    const uint256& convergence_root = {})
{
    proof.convergence_txid = convergence_txid;
    proof.convergence_amount_sat = static_cast<uint64_t>(convergence_amt);
    proof.bind_hash = bind;
    proof.height = height;
    proof.target_prev = target_prev;
    proof.convergence_root = convergence_root;
}

void PushPcorbCarrier(CMutableTransaction& coinbase, const Consensus::ConvergeProof& proof)
{
    coinbase.vout.push_back(BuildPcorbCarrierOutput(
        proof.convergence_txid,
        static_cast<CAmount>(proof.convergence_amount_sat),
        proof.bind_hash,
        proof.height,
        proof.target_prev,
        proof.convergence_root));
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(res_proof_math_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(res_proof_verify_true_on_valid_vector)
{
    const uint256 bind = uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    const uint64_t height = 101;
    const uint32_t target_prev = 0x1f00ffff;
    const CAmount convergence_amt = COIN;
    uint256 convergence_txid;
    BOOST_REQUIRE(FindPassingConvergenceTxid(convergence_txid, bind, height, convergence_amt));

    const uint256 convergence_root{};
    Consensus::ConvergeProof proof;
    FillProofContext(proof, convergence_txid, convergence_amt, bind, height, target_prev, convergence_root);

    // VerifyConvergeProof must agree with the raw inequality on the same σ: the verifier is a
    // wrapper over `σ < T·f`, not an independent rule.
    const uint256 digest = Consensus::ConvergeProofDigest(convergence_txid, bind, height, convergence_root);
    BOOST_CHECK(Consensus::ConvergenceDrawInequalityHolds(
        digest, Params().GetConsensus().RES_PROBABILITY_TARGET, convergence_amt));
    BOOST_CHECK(Consensus::VerifyConvergeProof(proof, Params().GetConsensus().RES_PROBABILITY_TARGET, &convergence_txid, nullptr));
    const uint256 expected_fr = convergence_root;
    BOOST_CHECK(Consensus::VerifyConvergeProof(proof, Params().GetConsensus().RES_PROBABILITY_TARGET, &convergence_txid, &expected_fr));
}

BOOST_AUTO_TEST_CASE(res_proof_wrong_consensus_bind_is_rejected)
{
    const uint256 bind_a = uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    const uint256 bind_b = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    const uint64_t height = 55;
    const uint32_t target_prev = 0x1f00ffff;
    const CAmount convergence_amt = COIN;
    uint256 convergence_txid;
    BOOST_REQUIRE(FindPassingConvergenceTxid(convergence_txid, bind_a, height, convergence_amt));

    const uint256 convergence_root{};

    // The rejection below must come from σ actually moving with the bind, not from some
    // incidental check inside the verifier.
    const uint256 digest_a = Consensus::ConvergeProofDigest(convergence_txid, bind_a, height, convergence_root);
    const uint256 digest_b = Consensus::ConvergeProofDigest(convergence_txid, bind_b, height, convergence_root);
    BOOST_CHECK(digest_a != digest_b);

    Consensus::ConvergeProof proof;
    FillProofContext(proof, convergence_txid, convergence_amt, bind_a, height, target_prev, convergence_root);
    BOOST_CHECK(Consensus::VerifyConvergeProof(proof, Params().GetConsensus().RES_PROBABILITY_TARGET, &convergence_txid, nullptr));
    FillProofContext(proof, convergence_txid, convergence_amt, bind_b, height, target_prev, convergence_root);
    BOOST_CHECK(!Consensus::VerifyConvergeProof(proof, Params().GetConsensus().RES_PROBABILITY_TARGET, &convergence_txid, nullptr));
}

BOOST_AUTO_TEST_CASE(res_proof_tamper_convergence_fails)
{
    const uint256 bind = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    const uint64_t height = 55;
    const uint32_t target_prev = 0x1f00ffff;
    const CAmount convergence_amt = COIN;
    uint256 convergence_txid;
    BOOST_REQUIRE(FindPassingConvergenceTxid(convergence_txid, bind, height, convergence_amt));

    const uint256 convergence_root{};
    const uint256 digest = Consensus::ConvergeProofDigest(convergence_txid, bind, height, convergence_root);
    Consensus::ConvergeProof proof;
    FillProofContext(proof, convergence_txid, convergence_amt, bind, height, target_prev, convergence_root);

    proof.convergence_txid.begin()[0] ^= 0x01;
    BOOST_CHECK(Consensus::ConvergeProofDigest(proof.convergence_txid, bind, height, convergence_root) != digest);
    BOOST_CHECK(!Consensus::VerifyConvergeProof(proof, Params().GetConsensus().RES_PROBABILITY_TARGET, &convergence_txid, nullptr));
}

BOOST_AUTO_TEST_CASE(res_proof_target_prev_is_outside_sigma_and_pinned_by_connectblock)
{
    // SECURITY-001-A8 moved `target_prev` out of the σ preimage. It used to be folded in via
    // `bind_commit` to keep two difficulties in separate draw domains; that job now belongs to
    // `bind_hash`, which is the parent block hash and therefore already commits the parent's
    // nBits. Folding the compact target in a second time bought nothing.
    //
    // So σ is deliberately blind to `target_prev`, and this case pins that. The field survives
    // in the witness only as a declaration, and the only thing keeping it honest is ConnectBlock
    // comparing it to the parent — see
    // `security001a_mint_proof_target_prev_must_equal_parent_nbits`.
    const uint256 bind = uint256S("0xdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    const uint64_t consensus_h = 12;
    const uint32_t consensus_target = 0x207fffff;
    const uint32_t fake_witness_target = 0x01003456;
    BOOST_REQUIRE(consensus_target != fake_witness_target);

    // Saturating weight so the assertions isolate the digest rather than draw luck.
    const CAmount convergence_amt = SATURATING_CONVERGENCE_AMOUNT;
    uint256 convergence_txid;
    BOOST_REQUIRE(FindPassingConvergenceTxid(convergence_txid, bind, consensus_h, convergence_amt));
    const uint256 convergence_root{};

    Consensus::ConvergeProof proof;
    const uint256& res_probability_target = Params().GetConsensus().RES_PROBABILITY_TARGET;

    // Both proofs pass the stateless check, and they produce the same σ: the digest cannot see
    // the difference. Neither reaches a block, because A-3 pins target_prev to the parent.
    FillProofContext(proof, convergence_txid, convergence_amt, bind, consensus_h, fake_witness_target, convergence_root);
    BOOST_CHECK(Consensus::VerifyConvergeProof(proof, res_probability_target, &convergence_txid, nullptr));

    FillProofContext(proof, convergence_txid, convergence_amt, bind, consensus_h, consensus_target, convergence_root);
    BOOST_CHECK(Consensus::VerifyConvergeProof(proof, res_probability_target, &convergence_txid, nullptr));
}

// SECURITY-001-A8: a candidate's draw domain must not survive being moved to a fork. Under
// Sigma-PoW-01 the separator is `bind_hash`, the hash of the block that committed the
// convergence set, so a fork is a different parent hash and therefore a different σ — the
// candidate is re-drawn rather than carrying its win across.
//
// This is strictly stronger than the old `target_prev` fold it replaces, which only separated
// forks that happened to sit at different difficulties. Two competing tips at the same
// difficulty shared a draw domain; now they never can, because they are different blocks.
BOOST_AUTO_TEST_CASE(res_proof_sigma_pow01_cross_fork_replay_blocked)
{
    const uint256 bind_main = uint256S("0x1234123412341234123412341234123412341234123412341234123412341234");
    const uint256 bind_fork = uint256S("0x1234123412341234123412341234123412341234123412341234123412341235");
    BOOST_REQUIRE(bind_main != bind_fork);
    const uint64_t height = 77;

    const CAmount convergence_amt = COIN;
    const uint256 convergence_root = uint256S("0x9999999999999999999999999999999999999999999999999999999999999999");

    uint256 convergence_txid;
    BOOST_REQUIRE(FindPassingConvergenceTxid(convergence_txid, bind_main, height, convergence_amt, convergence_root));

    Consensus::ConvergeProof main_proof;
    FillProofContext(main_proof, convergence_txid, convergence_amt, bind_main, height, 0x207fffff, convergence_root);
    BOOST_CHECK(Consensus::VerifyConvergeProof(main_proof, Params().GetConsensus().RES_PROBABILITY_TARGET, &convergence_txid, nullptr));

    // Attacker re-mounts the identical witness under a different parent. σ moves.
    Consensus::ConvergeProof replay_proof = main_proof;
    replay_proof.bind_hash = bind_fork;
    BOOST_CHECK(!Consensus::VerifyConvergeProof(replay_proof, Params().GetConsensus().RES_PROBABILITY_TARGET, &convergence_txid, nullptr));

    const uint256 digest_main = Consensus::ConvergeProofDigest(convergence_txid, bind_main, height, convergence_root);
    const uint256 digest_fork = Consensus::ConvergeProofDigest(convergence_txid, bind_fork, height, convergence_root);
    BOOST_CHECK(digest_main != digest_fork);
}

BOOST_AUTO_TEST_CASE(res_proof_parse_rejects_body_tx)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.hash = uint256::ONE;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 50 * COIN;
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_1;

    Consensus::ConvergeProof p;
    FillProofContext(p, uint256::ONE, COIN, uint256::ONE, 1u, 0x1f00ffff);
    PushPcorbCarrier(mtx, p);

    Consensus::ConvergeProof parsed;
    BOOST_CHECK(!Consensus::ParseConvergeProof(CTransaction(mtx), parsed));
}

BOOST_AUTO_TEST_CASE(res_proof_parse_rejects_leftover_witness)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.hash = uint256::ONE;
    mtx.vin[0].prevout.n = 0;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 1;
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_1;

    mtx.vin[0].scriptWitness.stack.emplace_back(32, 0);
    mtx.vin[0].scriptWitness.stack.emplace_back(8, 0);
    mtx.vin[0].scriptWitness.stack.emplace_back(32, 0);
    mtx.vin[0].scriptWitness.stack.emplace_back(8, 0);
    mtx.vin[0].scriptWitness.stack.emplace_back(4, 0);
    mtx.vin[0].scriptWitness.stack.emplace_back(32, 0);
    Consensus::ConvergeProof p1;
    BOOST_CHECK(!Consensus::ParseConvergeProof(CTransaction(mtx), p1));
}

BOOST_AUTO_TEST_CASE(res_proof_parse_accepts_coinbase_carrier)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 1;
    mtx.vout.emplace_back(50 * COIN, CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11));

    const uint256 bind = uint256S("0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    const uint32_t height = 3;
    const uint32_t target_prev = 0x1f00ffff;
    const CAmount convergence_amt = COIN;
    uint256 convergence_txid;
    BOOST_REQUIRE(FindPassingConvergenceTxid(convergence_txid, bind, height, convergence_amt));
    const uint256 convergence_root{};
    const uint256 digest = Consensus::ConvergeProofDigest(convergence_txid, bind, height, convergence_root);
    Consensus::ConvergeProof proof;
    FillProofContext(proof, convergence_txid, convergence_amt, bind, height, target_prev, convergence_root);
    PushPcorbCarrier(mtx, proof);

    const CTransaction tx(mtx);
    BOOST_REQUIRE(tx.IsCoinBase());
    Consensus::ConvergeProof parsed;
    BOOST_REQUIRE(Consensus::ParseConvergeProof(tx, parsed));
    BOOST_CHECK_EQUAL(parsed.convergence_txid, convergence_txid);
    BOOST_CHECK(Consensus::ConvergeProofDigest(
                    parsed.convergence_txid, parsed.bind_hash, parsed.height, parsed.convergence_root) == digest);
    BOOST_CHECK(Consensus::VerifyConvergeProof(parsed, Params().GetConsensus().RES_PROBABILITY_TARGET, &convergence_txid, nullptr));
    BOOST_CHECK_EQUAL(tx.vout.back().scriptPubKey.size(), RigorTags::PCORB_CARRIER_SCRIPT_BYTES);
}

BOOST_AUTO_TEST_CASE(res_proof_parse_rejects_bad_payload)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 1;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 0;
    std::vector<unsigned char> payload(RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES, 0);
    std::copy(RigorTags::PCOR.begin(), RigorTags::PCOR.end(), payload.begin());
    payload.resize(RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES - 1);
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << payload;
    Consensus::ConvergeProof p1;
    BOOST_CHECK(!Consensus::ParseConvergeProof(CTransaction(mtx), p1));

    payload.assign(RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES, 0);
    payload[0] = 'X';
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << payload;
    BOOST_CHECK(!Consensus::ParseConvergeProof(CTransaction(mtx), p1));

    mtx.vout[0].nValue = 1;
    payload.assign(RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES, 0);
    std::copy(RigorTags::PCOR.begin(), RigorTags::PCOR.end(), payload.begin());
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_MARKER_RETURN << payload;
    BOOST_CHECK(!Consensus::ParseConvergeProof(CTransaction(mtx), p1));
}

BOOST_AUTO_TEST_CASE(res_proof_two_carriers_are_not_a_proof)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 1;
    Consensus::ConvergeProof proof;
    FillProofContext(proof, uint256::ONE, COIN, uint256::ONE, 1u, 0x1f00ffff);
    PushPcorbCarrier(mtx, proof);
    PushPcorbCarrier(mtx, proof);
    Consensus::ConvergeProof p1;
    BOOST_CHECK(!Consensus::ParseConvergeProof(CTransaction(mtx), p1));
    BOOST_CHECK_EQUAL(Consensus::CountPcorbCarrierOutputs(CTransaction(mtx)), 2U);
}

BOOST_AUTO_TEST_CASE(res_proof_parse_empty_coinbase)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 1;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 1;
    mtx.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_1;

    Consensus::ConvergeProof p1;
    BOOST_CHECK(!Consensus::ParseConvergeProof(CTransaction(mtx), p1));
}

BOOST_AUTO_TEST_CASE(res_proof_digest_stable)
{
    const uint256 convergence = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    const uint256 bind = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
    const uint256 convergence_root{};
    const uint256 d = Consensus::ConvergeProofDigest(convergence, bind, 7u, convergence_root);
    BOOST_CHECK(d == Consensus::ConvergeProofDigest(convergence, bind, 7u, convergence_root));
}

BOOST_AUTO_TEST_CASE(res_proof_digest_changes_with_convergence_root)
{
    const uint256 convergence = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    const uint256 bind = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
    const uint256 fr_a = uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    const uint256 fr_b = uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    const uint256 d_a = Consensus::ConvergeProofDigest(convergence, bind, 7u, fr_a);
    const uint256 d_b = Consensus::ConvergeProofDigest(convergence, bind, 7u, fr_b);
    BOOST_CHECK(d_a != d_b);
}

BOOST_AUTO_TEST_CASE(res_proof_expected_convergence_root_mismatch_rejects)
{
    const uint256 bind = uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    const uint64_t height = 101;
    const uint32_t target_prev = 0x1f00ffff;
    const CAmount convergence_amt = COIN;
    uint256 convergence_txid;
    const uint256 proof_convergence_root = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    BOOST_REQUIRE(FindPassingConvergenceTxid(convergence_txid, bind, height, convergence_amt, proof_convergence_root));

    Consensus::ConvergeProof proof;
    FillProofContext(proof, convergence_txid, convergence_amt, bind, height, target_prev, proof_convergence_root);

    const uint256 wrong_expected = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
    // The expected-root argument is a separate gate from σ: the proof is internally consistent
    // and only fails because the caller demands a root it did not commit to.
    const uint256 digest = Consensus::ConvergeProofDigest(convergence_txid, bind, height, proof_convergence_root);
    BOOST_CHECK(Consensus::ConvergenceDrawInequalityHolds(
        digest, Params().GetConsensus().RES_PROBABILITY_TARGET, convergence_amt));
    BOOST_CHECK(!Consensus::VerifyConvergeProof(proof, Params().GetConsensus().RES_PROBABILITY_TARGET, &convergence_txid, &wrong_expected));
    BOOST_CHECK(Consensus::VerifyConvergeProof(proof, Params().GetConsensus().RES_PROBABILITY_TARGET, &convergence_txid, &proof_convergence_root));
}
BOOST_AUTO_TEST_CASE(security001a_eligibility_filter_is_not_vacuous)
{
    // SECURITY-001-A finding A-6. RFC-014 §2.1 shipped T = 2^240 while evaluating its own
    // rule `H_i < T·f_i` at f_i = 1, but f_i is denominated in satoshis with a 1 ORU floor.
    // The result was T·f_min > 2^256, i.e. an eligibility filter that returned true for
    // every candidate. This case pins the corrected constant and the invariant that makes
    // the filter meaningful at all.
    const uint256& T = Params().GetConsensus().RES_PROBABILITY_TARGET;
    BOOST_CHECK(T == Consensus::RES_PROBABILITY_TARGET_CONST);

    // Non-vacuity: T · MIN_CONVERGENCE_AMOUNT must not overflow past 2^256, otherwise the
    // 512-bit comparison is satisfied by every possible σ.
    const Consensus::uint512 floor_product = Consensus::rigor_math_detail::Mul256x256Full(
        UintToArith256(T), arith_uint256(static_cast<uint64_t>(Consensus::MIN_CONVERGENCE_AMOUNT)));
    const Consensus::uint512 two_pow_256 = Consensus::rigor_math_detail::Mul256x256Full(
        arith_uint256(1) << 255, arith_uint256(2));
    BOOST_CHECK(floor_product < two_pow_256);

    // σ = all ones is the largest representable digest; at the minimum weight it must lose.
    uint256 sigma_max;
    sigma_max.SetHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    BOOST_CHECK(!Consensus::ConvergenceDrawInequalityHolds(sigma_max, T, Consensus::MIN_CONVERGENCE_AMOUNT));

    // σ = 0 is the smallest; it must win at the minimum weight, so the filter is not
    // uniformly false either.
    BOOST_CHECK(Consensus::ConvergenceDrawInequalityHolds(uint256{}, T, Consensus::MIN_CONVERGENCE_AMOUNT));

    // Calibration pin. RFC-014 §2.1 intent is p(n ORU) = n / C in EXPECTED WINS PER CANDIDATE,
    // with C = 2^(256-E) = 1024 at the RFC-017 exponent; SECURITY-001-A9-C spreads that over W
    // draws, so the per-draw figure is n / (C·W). A candidate above the per-draw saturation
    // point must be eligible regardless of its digest.
    BOOST_CHECK(Consensus::ConvergenceDrawInequalityHolds(sigma_max, T, SATURATING_CONVERGENCE_AMOUNT));

    // SECURITY-001-A9-C: T is the pre-windowing target divided by the window, floored. This is
    // what makes issuance neutral — a candidate now sits in W draws at 1/W the odds each, so
    // expected wins per unit burned is unchanged.
    //
    // The division is not lossless: the pre-windowing target's residue mod 32 is 27 at E = 246,
    // so the floor discards it. The resulting deviation from exact neutrality is a relative
    // 2.4e-65, so the floor relation is what gets pinned rather than a lossless round trip.
    const arith_uint256 unwindowed = UintToArith256(Consensus::RES_PROBABILITY_TARGET_UNWINDOWED);
    const arith_uint256 window{static_cast<uint64_t>(Consensus::CONVERGENCE_DRAW_WINDOW)};
    BOOST_CHECK(UintToArith256(T) == (unwindowed >> Consensus::CONVERGENCE_DRAW_WINDOW_LOG2));
    BOOST_CHECK((UintToArith256(T) * window) <= unwindowed);
    BOOST_CHECK(unwindowed < ((UintToArith256(T) + 1) * window));

    // The window must actually widen the candidate set; W = 1 would restore the F-1 defect of
    // a single miner choosing F unilaterally.
    BOOST_CHECK(Consensus::CONVERGENCE_DRAW_WINDOW > 1);
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW, 1 << Consensus::CONVERGENCE_DRAW_WINDOW_LOG2);

    // The pre-windowing saturation point C is no longer a per-draw guarantee — that removal is
    // the point of A-9-C (design review finding F-2: "burn to saturation, then censor"). C is
    // 1024 ORU at the RFC-017 exponent E = 246.
    BOOST_CHECK(!Consensus::ConvergenceDrawInequalityHolds(sigma_max, T, 1024 * COIN));

    // Per-draw saturation sits just ABOVE C·W = 32 768 ORU, not at it: T is floored twice, so
    // T·(C·W·COIN) lands a hair under 2^256 and the maximal digest is still rejected there.
    // Any strictly larger weight is unconditionally eligible.
    BOOST_CHECK(!Consensus::ConvergenceDrawInequalityHolds(sigma_max, T, 32768 * COIN));
    BOOST_CHECK(Consensus::ConvergenceDrawInequalityHolds(sigma_max, T, 65536 * COIN));
}

// =============================================================================
namespace {

const CScript g_sec001a_oru_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x22);
/** The declared RES owner's 32-byte program. */
const std::vector<unsigned char> g_sec001a_owner_program(32, 0x77);

CScript HonestMintPayoutScript(int block_height)
{
    return GetResMintScriptForPubKey(
        static_cast<int64_t>(block_height) + Params().GetConsensus().nCoinbaseMaturity,
        g_sec001a_owner_program);
}

/**
 * Minimal ConnectBlock-shaped fixture for the mint evaluator.
 *
 * SECURITY-001-A8: the mint at height h is checked against the convergence set committed by
 * block h-1, so the fixture builds that parent set directly rather than putting the convergence
 * in the same block as the mint. `expected_bind` stands in for the parent's hash — the σ seed.
 */
struct Security001AFixture {
    CCoinsView base;
    CCoinsViewCache coins{&base};
    CTransactionRef convergence_tx;
    uint256 convergence_txid;
    // Saturating weight: keeps these binding tests independent of the draw odds, so they
    // stay meaningful if RES_PROBABILITY_TARGET is ever retuned again.
    CAmount convergence_amount{SATURATING_CONVERGENCE_AMOUNT};
    // Where the mint pays. Defaults to the owner the convergence declares, so tests that are not
    // about A-5 stay honest; A-5's own case overrides it.
    CScript res_payout_spk;

    // Chain context ConnectBlock derives once and hands to the mint evaluator.
    uint256 expected_bind{uint256S("0x5555555555555555555555555555555555555555555555555555555555555555")};
    uint32_t expected_target_prev{0x207fffff};
    int block_height{200};

    ConvergenceDrawSet parent_draw_set;

    Security001AFixture()
    {
        res_payout_spk = HonestMintPayoutScript(block_height);
        const COutPoint prev_conv{uint256S("0xaaaabbbb00000000000000000000000000000000000000000000000000000000"), 0};
        const COutPoint prev_mint{uint256S("0xaaaabbbb11111111111111111111111111111111111111111111111111111111"), 0};
        coins.AddCoin(prev_conv, Coin(CTxOut{convergence_amount + 2 * COIN, g_sec001a_oru_spk}, 100, false), false);
        coins.AddCoin(prev_mint, Coin(CTxOut{20 * COIN, g_sec001a_oru_spk}, 100, false), false);

        CMutableTransaction conv;
        conv.vin.resize(1);
        conv.vin[0].prevout = prev_conv;
        conv.vout.resize(2);
        conv.vout[0].nValue = convergence_amount;
        conv.vout[0].scriptPubKey = GetConvergeScriptForPubKey(g_sec001a_owner_program);
        conv.vout[1].nValue = COIN;
        conv.vout[1].scriptPubKey = g_sec001a_oru_spk;
        convergence_tx = MakeTransactionRef(conv);
        convergence_txid = convergence_tx->GetHash();

        parent_draw_set.source_block_hash = expected_bind;
        parent_draw_set.source_height = block_height - 1;
        parent_draw_set.source_nbits = expected_target_prev;
        parent_draw_set.convergence_root =
            Consensus::ComputeConvergenceRootFromEligibilityTxids({convergence_txid});
        parent_draw_set.candidates.push_back(ConvergenceDrawCandidate{
            convergence_txid, static_cast<uint64_t>(convergence_amount), Hash160(g_sec001a_owner_program)});
    }

    uint256 ConvergenceRoot() const { return parent_draw_set.convergence_root; }

    /** Build a coinbase whose RIGOR_MARKER_RES_MINT output pays `res_payout_spk`. */
    CTransactionRef MakeMintTx(CAmount res_amount = Consensus::RES_REWARD_GENESIS)
    {
        CMutableTransaction mint;
        mint.vin.resize(1);
        mint.vin[0].prevout.SetNull();
        mint.vin[0].scriptSig = CScript() << block_height;
        mint.vout.emplace_back(50 * COIN, g_sec001a_oru_spk);
        mint.vout.emplace_back(res_amount, res_payout_spk);
        return MakeTransactionRef(mint);
    }

    std::string Evaluate(bool* ok_out = nullptr)
    {
        return EvaluateTx(*MakeMintTx(), ok_out);
    }

    std::string EvaluateTx(const CTransaction& mint, bool* ok_out = nullptr)
    {
        const ConvergenceDrawOutcome outcome =
            ResolveConvergenceDraw(parent_draw_set, Params().GetConsensus().RES_PROBABILITY_TARGET);

        BlockValidationState state;
        CAmount minted = 0;
        const bool ok = rigor::ConnectEvaluateCoinbaseResMint(mint, block_height,
            Params().GetConsensus(), parent_draw_set, outcome, state, minted);
        if (ok_out != nullptr) *ok_out = ok;
        return state.GetRejectReason();
    }

    /** A proof that agrees with every chain-derived value. */
    Consensus::ConvergeProof HonestProof() const
    {
        Consensus::ConvergeProof proof;
        FillProofContext(proof, convergence_txid, convergence_amount, expected_bind,
            static_cast<uint64_t>(block_height), expected_target_prev, ConvergenceRoot());
        return proof;
    }
};

} // namespace

BOOST_AUTO_TEST_CASE(security001a_binding_requires_exact_on_chain_convergence_amount)
{
    Security001AFixture f;

    // The committed weight comes from the parent's draw set, which is where ConnectBlock reads
    // it since A-8; the proof only gets to agree with it.
    const CAmount committed = static_cast<CAmount>(f.parent_draw_set.candidates[0].weight);

    Consensus::ConvergeProof proof;
    FillProofContext(proof, f.convergence_txid, f.convergence_amount, f.expected_bind,
        static_cast<uint64_t>(f.block_height), f.expected_target_prev, f.ConvergenceRoot());
    BOOST_CHECK(Consensus::CheckConvergeProofBinding(proof, committed));

    // Inflated weight: previously accepted because only `>= MIN_CONVERGENCE_AMOUNT` was enforced,
    // which handed the attacker a free multiplier in both σ < T·f and the argmin H/f comparison.
    proof.convergence_amount_sat = static_cast<uint64_t>(f.convergence_amount) * 1000;
    BOOST_CHECK(!Consensus::CheckConvergeProofBinding(proof, committed));

    proof.convergence_amount_sat = static_cast<uint64_t>(MAX_MONEY);
    BOOST_CHECK(!Consensus::CheckConvergeProofBinding(proof, committed));

    // Deflated but still above the floor.
    proof.convergence_amount_sat = static_cast<uint64_t>(f.convergence_amount) + 1;
    BOOST_CHECK(!Consensus::CheckConvergeProofBinding(proof, committed));

    // Out-of-range values must fail closed rather than wrap through the narrowing cast.
    proof.convergence_amount_sat = ~uint64_t{0};
    BOOST_CHECK(!Consensus::CheckConvergeProofBinding(proof, committed));

    proof.convergence_amount_sat = static_cast<uint64_t>(f.convergence_amount);
    BOOST_CHECK(Consensus::CheckConvergeProofBinding(proof, committed));
}

BOOST_AUTO_TEST_CASE(security001a8_draw_set_must_be_the_immediate_parent)
{
    // The set is only valid for exactly one payout height. Resolving a stale set at a later
    // height would let a producer re-present an old winner.
    Security001AFixture f;
    f.parent_draw_set.source_height = f.block_height - 2;

    BOOST_CHECK_EQUAL(f.Evaluate(), "bad-rigor-res-draw-set-height");
}

BOOST_AUTO_TEST_CASE(security001a_honest_mint_proof_still_accepted)
{
    // Liveness guard: the three bindings above must be a no-op for a template built the way
    // node/auto_res_mint.cpp builds one, otherwise the remediation would invalidate honest blocks.
    Security001AFixture f;

    bool ok{false};
    const std::string reason = f.Evaluate(&ok);
    BOOST_CHECK_MESSAGE(ok, "honest RES mint rejected: " + reason);
    BOOST_CHECK_EQUAL(reason, "");
}

BOOST_AUTO_TEST_CASE(security001a_res_reward_must_pay_declared_owner)
{
    // SECURITY-001-A finding A-5. The proof bindings above all verify *which convergence* the
    // mint refers to; none of them constrained *who gets paid*. A block producer could take a
    // user's winning convergence, build an otherwise perfect proof for it, and send the RES to
    // itself. This is that block, end to end through the evaluator.
    Security001AFixture f;

    f.res_payout_spk = GetResMintScriptForPubKey(
        static_cast<int64_t>(f.block_height) + Params().GetConsensus().nCoinbaseMaturity,
        std::vector<unsigned char>(32, 0xbb));
    BOOST_CHECK_EQUAL(f.Evaluate(), "bad-rigor-res-owner-mismatch");

    f.res_payout_spk = HonestMintPayoutScript(f.block_height);
    BOOST_CHECK_EQUAL(f.Evaluate(), "");
}

BOOST_AUTO_TEST_CASE(security001a_mint_amount_must_equal_res_reward)
{
    Security001AFixture f;
    const CTransactionRef wrong = f.MakeMintTx(Consensus::RES_REWARD_GENESIS * 2);
    BOOST_CHECK_EQUAL(f.EvaluateTx(*wrong), "bad-rigor-res-amount");
    const CTransactionRef dust = f.MakeMintTx(RES_COIN);
    BOOST_CHECK_EQUAL(f.EvaluateTx(*dust), "bad-rigor-res-amount");
}

BOOST_AUTO_TEST_CASE(security001a_mint_without_winner_is_rejected)
{
    Security001AFixture f;
    f.parent_draw_set.candidates.clear();
    f.parent_draw_set.convergence_root = Consensus::ComputeConvergenceRootFromEligibilityTxids({});
    BOOST_CHECK_EQUAL(f.Evaluate(), "bad-rigor-res-without-winner");
}

BOOST_AUTO_TEST_CASE(rfc017_exhausted_schedule_rejects_mint)
{
    // H-1: after epoch 250, W(h) = 0. A mint is not authorized even if the draw named a
    // winner. ConnectBlock must then accept mint_count == 0 (validation.cpp mint-auth gate);
    // this case pins the evaluator fail-closed side.
    Security001AFixture f;
    f.block_height = Consensus::RES_REWARD_EPOCH * (Consensus::RES_REWARD_LAST_EPOCH + 1);
    f.parent_draw_set.source_height = f.block_height - 1;
    // Constructor baked maturity into the payout script at height 200. Rebuild so the
    // mint-script gate does not fire first and mask the W(h)=0 amount reject.
    f.res_payout_spk = HonestMintPayoutScript(f.block_height);
    BOOST_CHECK_EQUAL(Consensus::GetResMintReward(f.block_height), 0);
    BOOST_CHECK_EQUAL(f.Evaluate(), "bad-rigor-res-amount");
}

BOOST_AUTO_TEST_CASE(coinbase_two_mint_payouts_rejected)
{
    Security001AFixture f;
    CMutableTransaction mint(*f.MakeMintTx());
    mint.vout.push_back(mint.vout[1]); // second RIGOR_MARKER_RES_MINT
    BOOST_CHECK_EQUAL(f.EvaluateTx(CTransaction(mint)), "bad-rigor-res-amount");
}

/** Evaluator non-claim: omitting RIGOR_MARKER_RES_MINT is allowed here.
 *  ConnectBlock winner-must-mint is `bad-rigor-res-winner-mismatch`
 *  (`feature_rigor_convergence_rng_consensus.py`). Coq `winner_must_mint`
 *  is not CI-033. */
BOOST_AUTO_TEST_CASE(mint_auth_omit_is_not_evaluator_gate)
{
    Security001AFixture f;
    CMutableTransaction mint(*f.MakeMintTx());
    mint.vout.erase(mint.vout.begin() + 1);
    bool ok{false};
    BOOST_CHECK_EQUAL(f.EvaluateTx(CTransaction(mint), &ok), "");
    BOOST_CHECK(ok);
}

BOOST_AUTO_TEST_SUITE_END()
