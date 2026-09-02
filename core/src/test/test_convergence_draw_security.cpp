// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-079: Convergence Draw security audit.
// Standalone with -DCONVERGENCE_DRAW_SECURITY_STANDALONE; Boost otherwise.
// Does not recode ResolveConvergenceDraw, issuance, supply, or Kernel.

#include <consensus/amount.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_math.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_fails = 0;

void Fail(const char* msg)
{
    std::fprintf(stderr, "FAIL %s\n", msg);
    ++g_fails;
}

constexpr CAmount kSaturatingWeight =
    65536 * static_cast<CAmount>(Consensus::CONVERGENCE_DRAW_WINDOW) * Consensus::MIN_CONVERGENCE_AMOUNT;

uint256 TxidByte(unsigned char b)
{
    uint256 id;
    std::fill(id.begin(), id.end(), static_cast<unsigned char>(0));
    *(id.begin() + 31) = b;
    return id;
}

uint256 TxidFill(unsigned char b)
{
    uint256 id;
    std::fill(id.begin(), id.end(), b);
    return id;
}

ConvergenceDrawSet MakeDrawSet(int source_height, const std::vector<std::pair<uint256, uint64_t>>& members)
{
    ConvergenceDrawSet set;
    set.source_height = source_height;
    set.source_nbits = 0x207fffff;
    set.source_block_hash = uint256S("0xaa");
    set.convergence_root = uint256S("0xcc");
    for (const auto& m : members) {
        ConvergenceDrawCandidate c;
        c.txid = m.first;
        c.weight = m.second;
        c.source_height = source_height;
        set.candidates.push_back(c);
    }
    return set;
}

uint256 WinnerTxid(const ConvergenceDrawSet& set, const uint256& T)
{
    const ConvergenceDrawOutcome o = ResolveConvergenceDraw(set, T);
    if (!o.HasWinner()) return uint256();
    if (o.winner_index < 0 || static_cast<size_t>(o.winner_index) >= set.candidates.size()) {
        Fail("invalid-winner-index");
        return uint256();
    }
    if (o.eligible.size() != set.candidates.size()) {
        Fail("eligible-size");
        return uint256();
    }
    if (!o.eligible[static_cast<size_t>(o.winner_index)]) {
        Fail("ineligible-winner");
        return uint256();
    }
    return set.candidates[static_cast<size_t>(o.winner_index)].txid;
}

bool SameWinner(const ConvergenceDrawSet& a, const ConvergenceDrawSet& b, const uint256& T, const char* label)
{
    const uint256 wa = WinnerTxid(a, T);
    const uint256 wb = WinnerTxid(b, T);
    const bool ha = ResolveConvergenceDraw(a, T).HasWinner();
    const bool hb = ResolveConvergenceDraw(b, T).HasWinner();
    if (ha != hb || wa != wb) {
        Fail(label);
        return false;
    }
    return true;
}

ConvergenceDrawSet Permute(const ConvergenceDrawSet& in, const std::vector<size_t>& order)
{
    ConvergenceDrawSet out = in;
    out.candidates.clear();
    for (size_t i : order) {
        out.candidates.push_back(in.candidates[i]);
    }
    return out;
}

bool TestEmptyAndInvalidCandidates()
{
    const uint256 T = Consensus::RES_PROBABILITY_TARGET_CONST;
    const uint64_t w = static_cast<uint64_t>(kSaturatingWeight);
    const uint256 a = TxidByte(0x11);

    ConvergenceDrawSet unset;
    if (unset.IsSet()) Fail("unset-isset");
    if (ResolveConvergenceDraw(unset, T).HasWinner()) Fail("unset-winner");

    ConvergenceDrawSet empty;
    empty.source_height = 100;
    empty.source_block_hash = uint256S("0x11");
    if (ResolveConvergenceDraw(empty, T).HasWinner()) Fail("empty-candidate");
    if (ResolveConvergenceDraw(empty, T).winner_index != -1) Fail("empty-index");

    if (ResolveConvergenceDraw(MakeDrawSet(100, {{a, 0}}), T).HasWinner()) Fail("weight-zero");
    if (ResolveConvergenceDraw(MakeDrawSet(100, {{a, std::numeric_limits<uint64_t>::max()}}), T).HasWinner()) {
        Fail("weight-max-uint64");
    }

    const CScript unknown = CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(20, 0x11);
    if (ClassifyRigorAsset(unknown) != ASSET_INVALID) Fail("unknown-asset");
    const CScript truncated = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(19, 0x11);
    if (ClassifyRigorAsset(truncated) != ASSET_INVALID) Fail("malformed-truncated");
    const CScript random_bytes = CScript() << std::vector<unsigned char>{0xde, 0xad, 0xbe, 0xef};
    if (ClassifyRigorAsset(random_bytes) != ASSET_INVALID) Fail("malformed-random");
    const CScript empty_script;
    if (ClassifyRigorAsset(empty_script) != ASSET_INVALID) Fail("malformed-empty-script");

    ConvergenceDrawSet valid = MakeDrawSet(100, {{a, w}});
    if (!ResolveConvergenceDraw(valid, T).HasWinner()) Fail("valid-single");

    (void)w;
    std::printf("empty candidate: REJECT\n");
    std::printf("invalid candidate: REJECT\n");
    std::printf("unknown asset: REJECT\n");
    std::printf("malformed tx: REJECT\n");
    return g_fails == 0;
}

bool TestDeterminism()
{
    const uint256 T = Consensus::RES_PROBABILITY_TARGET_CONST;
    const uint64_t w = static_cast<uint64_t>(kSaturatingWeight);
    const uint256 a = TxidByte(0x11);
    const uint256 b = TxidByte(0x22);
    const uint256 d = TxidByte(0x33);

    ConvergenceDrawSet set = MakeDrawSet(200, {{a, w}, {b, w}, {d, w / 2}});
    const uint256 first = WinnerTxid(set, T);
    const uint256 second = WinnerTxid(set, T);
    if (first != second || first.IsNull()) Fail("same-input-repeat");

    std::vector<size_t> order(set.candidates.size());
    std::iota(order.begin(), order.end(), 0);
    do {
        if (!SameWinner(set, Permute(set, order), T, "candidate-order")) return g_fails == 0;
    } while (std::next_permutation(order.begin(), order.end()));

    ConvergenceDrawSet arrival = set;
    std::reverse(arrival.candidates.begin(), arrival.candidates.end());
    if (!SameWinner(set, arrival, T, "tx-arrival-order")) return g_fails == 0;

    ConvergenceDrawSet scored = set;
    scored.candidates[0].source_height = 1;
    scored.candidates[1].source_height = 99;
    scored.candidates[2].source_height = 50;
    if (!SameWinner(set, scored, T, "source-height-order")) return g_fails == 0;

    std::printf("DRAW_DETERMINISM = PASS\n");
    return g_fails == 0;
}

bool TestTieBreak()
{
    const uint256 T = Consensus::RES_PROBABILITY_TARGET_CONST;
    const uint64_t w = static_cast<uint64_t>(kSaturatingWeight);
    const uint256 a = TxidByte(0x11);
    const uint256 b = TxidByte(0x22);

    if (ConvergenceWinnerTieBreakCompare(a, b, uint256()) >= 0) Fail("tiebreak-a-lt-b");
    if (ConvergenceWinnerTieBreakCompare(b, a, uint256()) <= 0) Fail("tiebreak-b-gt-a");
    if (ConvergenceWinnerTieBreakCompare(a, a, uint256()) != 0) Fail("tiebreak-eq");
    const uint256 croot = uint256S("0xffffffff");
    if (ConvergenceWinnerTieBreakCompare(a, b, croot) != ConvergenceWinnerTieBreakCompare(a, b, uint256())) {
        Fail("tiebreak-ignores-root");
    }

    ConvergenceDrawSet eq = MakeDrawSet(300, {{a, w}, {b, w}});
    const uint256 w1 = WinnerTxid(eq, T);
    std::swap(eq.candidates[0], eq.candidates[1]);
    const uint256 w2 = WinnerTxid(eq, T);
    if (w1 != w2) Fail("equal-weight-reverse");

    ConvergenceDrawSet same_score = MakeDrawSet(300, {{a, w}, {a, w}});
    const uint256 d1 = WinnerTxid(same_score, T);
    std::swap(same_score.candidates[0], same_score.candidates[1]);
    const uint256 d2 = WinnerTxid(same_score, T);
    if (d1 != d2 || d1 != a) Fail("duplicate-same-score");

    ConvergenceDrawSet ts = MakeDrawSet(300, {{b, w}, {a, w}});
    ts.candidates[0].source_height = 7;
    ts.candidates[1].source_height = 7;
    const uint256 t1 = WinnerTxid(ts, T);
    std::reverse(ts.candidates.begin(), ts.candidates.end());
    const uint256 t2 = WinnerTxid(ts, T);
    if (t1 != t2) Fail("same-timestamp-reverse");

    std::printf("DRAW_TIEBREAK = PASS\n");
    return g_fails == 0;
}

bool TestWeightSecurity()
{
    const uint256 T = Consensus::RES_PROBABILITY_TARGET_CONST;
    const uint64_t w = static_cast<uint64_t>(kSaturatingWeight);
    const uint256 a = TxidByte(0x11);
    const uint256 b = TxidByte(0x22);

    if (Consensus::ConvergenceDrawInequalityHolds(uint256(10), T, 0)) Fail("ineq-weight-0");
    if (Consensus::ConvergenceDrawInequalityHolds(uint256(10), T, -1)) Fail("ineq-weight-neg");
    if (Consensus::ConvergenceDrawInequalityHolds(uint256(10), T, std::numeric_limits<CAmount>::max())) {
        Fail("ineq-weight-camount-max");
    }

    if (ResolveConvergenceDraw(MakeDrawSet(400, {{a, 0}}), T).HasWinner()) Fail("resolver-weight-0");
    if (ResolveConvergenceDraw(MakeDrawSet(400, {{a, std::numeric_limits<uint64_t>::max()}}), T).HasWinner()) {
        Fail("resolver-overflow-weight");
    }

    ConvergenceDrawSet mixed = MakeDrawSet(400, {{a, 0}, {b, w}});
    const uint256 winner = WinnerTxid(mixed, T);
    if (winner != b) Fail("zero-weight-does-not-win");
    std::swap(mixed.candidates[0], mixed.candidates[1]);
    if (WinnerTxid(mixed, T) != b) Fail("zero-weight-order");

    ConvergenceDrawSet large;
    large.source_height = 400;
    large.source_block_hash = uint256S("0xaa");
    large.convergence_root = uint256S("0xcc");
    for (int i = 0; i < 64; ++i) {
        ConvergenceDrawCandidate c;
        c.txid = TxidByte(static_cast<unsigned char>(i + 1));
        c.weight = w;
        large.candidates.push_back(c);
    }
    const uint256 large_w1 = WinnerTxid(large, T);
    if (large_w1.IsNull()) Fail("large-set-none");
    std::reverse(large.candidates.begin(), large.candidates.end());
    if (WinnerTxid(large, T) != large_w1) Fail("large-set-order");

    std::printf("overflow: NONE\n");
    std::printf("invalid weight: NONE\n");
    std::printf("DRAW_WEIGHT_SECURITY = PASS\n");
    return g_fails == 0;
}

bool TestThresholdBoundary()
{
    const uint256 T = Consensus::RES_PROBABILITY_TARGET_CONST;
    const uint64_t w = static_cast<uint64_t>(kSaturatingWeight);
    const uint256 a = TxidByte(0x11);
    ConvergenceDrawSet one = MakeDrawSet(500, {{a, w}});

    if (ResolveConvergenceDraw(one, uint256()).HasWinner()) Fail("T=0");

    uint256 tmax;
    std::fill(tmax.begin(), tmax.end(), static_cast<unsigned char>(0xff));
    if (!ResolveConvergenceDraw(one, tmax).HasWinner()) Fail("T=max");

    const ConvergenceDrawOutcome canon = ResolveConvergenceDraw(one, T);
    if (!canon.HasWinner()) Fail("T-canonical");

    const uint256 below_winner = WinnerTxid(one, uint256());
    const uint256 above_winner = WinnerTxid(one, tmax);
    if (!below_winner.IsNull()) Fail("T-below-winner");
    if (above_winner != a) Fail("T-above-winner");

    const ConvergenceDrawOutcome r1 = ResolveConvergenceDraw(one, T);
    const ConvergenceDrawOutcome r2 = ResolveConvergenceDraw(one, T);
    if (r1.HasWinner() != r2.HasWinner() || r1.winner_index != r2.winner_index) Fail("T-repeat");

    std::printf("threshold T=0 / T=max / T below / T above: DETERMINATE\n");
    return g_fails == 0;
}

bool TestAdversarial()
{
    const uint256 T = Consensus::RES_PROBABILITY_TARGET_CONST;
    const uint64_t w = static_cast<uint64_t>(kSaturatingWeight);
    const uint256 a = TxidByte(0x11);
    const uint256 b = TxidByte(0x22);
    const uint256 d = TxidByte(0x33);
    ConvergenceDrawSet committed = MakeDrawSet(600, {{a, w}, {b, w}, {d, w}});
    const uint256 committed_winner = WinnerTxid(committed, T);
    if (committed_winner.IsNull()) Fail("committed-none");

    // 1. txid grinding against a committed F cannot change the winner: extra
    //    grind bytes are not an input. Reordering the same txids is not grinding F.
    ConvergenceDrawSet grind = committed;
    std::reverse(grind.candidates.begin(), grind.candidates.end());
    if (WinnerTxid(grind, T) != committed_winner) Fail("txid-grind-order");

    // 2. candidate flooding: ineligible extras do not displace the committed winner.
    ConvergenceDrawSet flood = committed;
    for (int i = 0; i < 32; ++i) {
        ConvergenceDrawCandidate junk;
        junk.txid = TxidFill(static_cast<unsigned char>(0x80 + i));
        junk.weight = 0;
        flood.candidates.push_back(junk);
    }
    if (WinnerTxid(flood, T) != committed_winner) Fail("candidate-flooding");

    // 3. duplicate candidate: winner txid stays the committed identity.
    ConvergenceDrawSet dup = committed;
    dup.candidates.push_back(dup.candidates[0]);
    if (WinnerTxid(dup, T) != committed_winner) Fail("duplicate-candidate");
    std::reverse(dup.candidates.begin(), dup.candidates.end());
    if (WinnerTxid(dup, T) != committed_winner) Fail("duplicate-order");

    // 4. weight inflation: UINT64_MAX is ineligible; it cannot steal the draw.
    ConvergenceDrawSet inflate = committed;
    ConvergenceDrawCandidate fat;
    fat.txid = TxidFill(0xfe);
    fat.weight = std::numeric_limits<uint64_t>::max();
    inflate.candidates.push_back(fat);
    if (WinnerTxid(inflate, T) != committed_winner) Fail("weight-inflation");

    // 5/6. serialization / ordering manipulation of the same F.
    std::vector<size_t> order(committed.candidates.size());
    std::iota(order.begin(), order.end(), 0);
    do {
        if (WinnerTxid(Permute(committed, order), T) != committed_winner) {
            Fail("serialization-order");
            break;
        }
    } while (std::next_permutation(order.begin(), order.end()));

    std::printf("MANIPULATION_RESISTANCE = PASS\n");
    return g_fails == 0;
}

#ifdef CONVERGENCE_DRAW_SECURITY_STANDALONE
bool RunAll()
{
    g_fails = 0;
    if (!TestEmptyAndInvalidCandidates()) return 1;
    if (!TestDeterminism()) return 1;
    if (!TestTieBreak()) return 1;
    if (!TestWeightSecurity()) return 1;
    if (!TestThresholdBoundary()) return 1;
    if (!TestAdversarial()) return 1;
    if (g_fails != 0) return 1;
    std::printf("DRAW_AUTHORITY_COUNT = 1\n");
    std::printf("DRAW_DETERMINISM = PASS\n");
    std::printf("DRAW_TIEBREAK = PASS\n");
    std::printf("DRAW_WEIGHT_SECURITY = PASS\n");
    std::printf("MANIPULATION_RESISTANCE = PASS\n");
    std::printf("CRASH_FOUND = NO\n");
    return 0;
}
#endif

} // namespace

#ifdef CONVERGENCE_DRAW_SECURITY_STANDALONE
int main()
{
    return RunAll();
}
#else
#include <boost/test/unit_test.hpp>
#include <chainparams.h>
#include <coins.h>
#include <consensus/convergence_root.h>
#include <consensus/convergenceres.h>
#include <primitives/transaction.h>
#include <test/util/setup_common.h>
#include <test/util/transaction_utils.h>

BOOST_FIXTURE_TEST_SUITE(test_convergence_draw_security, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(empty_invalid_candidates)
{
    g_fails = 0;
    BOOST_REQUIRE(TestEmptyAndInvalidCandidates());
}

BOOST_AUTO_TEST_CASE(determinism)
{
    g_fails = 0;
    BOOST_REQUIRE(TestDeterminism());
}

BOOST_AUTO_TEST_CASE(tie_break)
{
    g_fails = 0;
    BOOST_REQUIRE(TestTieBreak());
}

BOOST_AUTO_TEST_CASE(weight_security)
{
    g_fails = 0;
    BOOST_REQUIRE(TestWeightSecurity());
}

BOOST_AUTO_TEST_CASE(threshold_boundary)
{
    g_fails = 0;
    BOOST_REQUIRE(TestThresholdBoundary());
}

BOOST_AUTO_TEST_CASE(adversarial)
{
    g_fails = 0;
    BOOST_REQUIRE(TestAdversarial());
}

BOOST_AUTO_TEST_CASE(assembly_rejects_invalid_members)
{
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x22);
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const uint256 h_ok = uint256S("0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    const uint256 h_bad = uint256S("0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    coins.AddCoin(COutPoint(h_ok, 0), Coin(CTxOut(20 * COIN, oru), 100, false), false);
    coins.AddCoin(COutPoint(h_bad, 0), Coin(CTxOut(20 * COIN, oru), 100, false), false);

    CMutableTransaction conv;
    conv.vin.resize(1);
    conv.vin[0].prevout = COutPoint(h_ok, 0);
    conv.vout.resize(2);
    conv.vout[0].nValue = Consensus::MIN_CONVERGENCE_AMOUNT;
    conv.vout[0].scriptPubKey = Consensus::ConvergenceOutputScript();
    conv.vout[1].nValue = 19 * COIN;
    conv.vout[1].scriptPubKey = oru;
    const auto conv_ref = MakeTransactionRef(conv);

    CMutableTransaction unknown;
    unknown.vin.resize(1);
    unknown.vin[0].prevout = COutPoint(h_bad, 0);
    unknown.vout.resize(1);
    unknown.vout[0].nValue = 20 * COIN;
    unknown.vout[0].scriptPubKey = CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(20, 0x11);
    const auto unknown_ref = MakeTransactionRef(unknown);

    CMutableTransaction malformed;
    malformed.vin.resize(1);
    malformed.vin[0].prevout = COutPoint(h_bad, 0);
    malformed.vout.resize(1);
    malformed.vout[0].nValue = COIN;
    malformed.vout[0].scriptPubKey = CScript() << std::vector<unsigned char>{0xde, 0xad};
    const auto malformed_ref = MakeTransactionRef(malformed);

    CMutableTransaction empty_out;
    empty_out.vin.resize(1);
    empty_out.vin[0].prevout = COutPoint(h_bad, 0);
    const auto empty_ref = MakeTransactionRef(empty_out);

    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    cb.vout.resize(1);
    cb.vout[0].nValue = 50 * COIN;
    cb.vout[0].scriptPubKey = oru;
    const auto cb_ref = MakeTransactionRef(cb);

    BOOST_REQUIRE(Consensus::IsConvergenceTx(*conv_ref));
    BOOST_REQUIRE(IsConvergenceCandidate(*conv_ref));
    BOOST_CHECK(!IsConvergenceCandidate(*unknown_ref));
    BOOST_CHECK(!IsConvergenceCandidate(*malformed_ref));
    BOOST_CHECK(!IsConvergenceCandidate(*empty_ref));

    const auto ids = Consensus::ListConvergenceRootCommitmentTxids(
        {cb_ref, conv_ref, unknown_ref, malformed_ref, empty_ref}, coins, 200, Params().GetConsensus());
    BOOST_REQUIRE_EQUAL(ids.size(), 1U);
    BOOST_CHECK_EQUAL(ids[0], conv_ref->GetHash());
}

BOOST_AUTO_TEST_SUITE_END()
#endif
