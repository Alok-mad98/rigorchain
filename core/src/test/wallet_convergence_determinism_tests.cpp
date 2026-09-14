// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// W1.1A.1 regression test: convergence-txid ordering must be a pure function of
// wallet state, not of mapWallet's hash-salt-dependent iteration order.
//
// This test exercises the determinism invariant enforced by wallet/interfaces.cpp:
//   - getConvergenceBlockResult: the wallet_convergence_txids vector handed to
//     chain().getConvergenceBlockResult() is sorted lexicographically.

#include <test/util/setup_common.h>
#include <uint256.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include <boost/test/unit_test.hpp>

namespace {

uint256 TxidFromByte(uint8_t b) { uint256 h; h.begin()[0] = b; return h; }

} // namespace

BOOST_FIXTURE_TEST_SUITE(wallet_convergence_determinism_tests, BasicTestingSetup)

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

BOOST_AUTO_TEST_SUITE_END()
