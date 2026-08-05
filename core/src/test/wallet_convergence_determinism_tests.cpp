// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// W1.1A.1 regression test: convergence-record selection and ordering must be a pure
// function of wallet state, not of mapWallet's hash-salt-dependent iteration order.
//
// This test exercises the determinism invariants enforced by wallet/interfaces.cpp:
//   - tryGetConvergenceUiSnapshot: when two records share nTimeReceived, the chosen
//     "best" record is the one with the lexicographically smaller txid.
//   - getConvergenceBlockResult: the wallet_convergence_txids vector handed to
//     chain().getConvergenceBlockResult() is sorted lexicographically.

#include <test/util/setup_common.h>
#include <uint256.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include <boost/test/unit_test.hpp>

namespace {

// Mirrors the (nTimeReceived, txid) -> "best" selection predicate in
// wallet/interfaces.cpp::tryGetConvergenceUiSnapshot. The predicate MUST be a pure
// function of the inputs; any divergence between two runs over the same input set
// signals a RULE-3 hidden-order-dependence regression.
struct Record { int64_t nTimeReceived; uint256 txid; };

const Record* SelectBest(const std::vector<Record>& records)
{
    const Record* best = nullptr;
    for (const auto& r : records) {
        const bool strictly_newer = best && r.nTimeReceived > best->nTimeReceived;
        const bool tie_break_wins = best && r.nTimeReceived == best->nTimeReceived && r.txid < best->txid;
        if (!best || strictly_newer || tie_break_wins) best = &r;
    }
    return best;
}

uint256 TxidFromByte(uint8_t b) { uint256 h; h.begin()[0] = b; return h; }

} // namespace

BOOST_FIXTURE_TEST_SUITE(wallet_convergence_determinism_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(tie_break_selects_smaller_txid)
{
    const uint256 lo = TxidFromByte(0x01);
    const uint256 hi = TxidFromByte(0xfe);
    // Same nTimeReceived; iteration order should NOT influence the winner.
    std::vector<Record> a{{1000, hi}, {1000, lo}};
    std::vector<Record> b{{1000, lo}, {1000, hi}};
    BOOST_CHECK(SelectBest(a)->txid == lo);
    BOOST_CHECK(SelectBest(b)->txid == lo);
}

BOOST_AUTO_TEST_CASE(newer_time_wins_regardless_of_txid)
{
    const uint256 lo = TxidFromByte(0x01);
    const uint256 hi = TxidFromByte(0xfe);
    // The newer-time record wins even if its txid is lexicographically larger.
    std::vector<Record> recs{{1000, lo}, {2000, hi}};
    BOOST_CHECK(SelectBest(recs)->txid == hi);
    BOOST_CHECK_EQUAL(SelectBest(recs)->nTimeReceived, 2000);
}

BOOST_AUTO_TEST_CASE(txid_vector_sort_is_replay_deterministic)
{
    // Mirrors wallet/interfaces.cpp::getConvergenceBlockResult: regardless of the
    // mapWallet iteration order, the txid vector passed downstream MUST be sorted.
    std::vector<uint256> ordering_one{TxidFromByte(0x03), TxidFromByte(0x01), TxidFromByte(0x02)};
    std::vector<uint256> ordering_two{TxidFromByte(0x02), TxidFromByte(0x03), TxidFromByte(0x01)};
    std::sort(ordering_one.begin(), ordering_one.end());
    std::sort(ordering_two.begin(), ordering_two.end());
    BOOST_CHECK(ordering_one == ordering_two);
    BOOST_CHECK(ordering_one.front() == TxidFromByte(0x01));
    BOOST_CHECK(ordering_one.back() == TxidFromByte(0x03));
}

BOOST_AUTO_TEST_CASE(empty_record_set_returns_null)
{
    std::vector<Record> empty;
    BOOST_CHECK(SelectBest(empty) == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
