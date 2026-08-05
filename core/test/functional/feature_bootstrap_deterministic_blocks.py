#!/usr/bin/env python3
# Copyright (c) 2026 The RigorChain developers
# SPDX-License-Identifier: MIT
"""Deterministic extension genesis → height 10 (devtest), and the A-8 draw seed at every height.

Asserts two synced nodes agree on block hashes 0..10.

SECURITY-001-A8 (Sigma-PoW-01) retired the RFC-000 §5 bootstrap window: σ used to bind to the
ancestor at h - 10, which is undefined for h < 10 and so needed a frozen preimage there. It now
binds to the parent block hash, which exists at every height from 1 up. This test therefore
asserts the opposite of what it used to: that heights 1..10 are not special-cased at all, and
that every block reports itself as the seed of the draw the next block pays.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class BootstrapDeterministicBlocksTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.chain = "devtest"
        self.extra_args = [[], []]

    def run_test(self):
        node_a, node_b = self.nodes
        addr = node_a.get_deterministic_witness_mining_address()
        self.generatetoaddress(node_a, 11, addr)
        self.sync_blocks(self.nodes, timeout=120)
        for n in self.nodes:
            n.syncwithvalidationinterfacequeue()

        assert_equal(node_a.getblockcount(), 11)
        assert_equal(node_b.getblockcount(), 11)
        for h in range(0, 11):
            assert_equal(node_a.getblockhash(h), node_b.getblockhash(h))

        for h in range(1, 11):
            bh = node_a.getblockhash(h)
            cr = node_a.getconvergenceresult(bh)
            assert_equal(cr["hash"], bh)
            assert_equal(cr["height"], h)
            # The seed is the block itself, so grinding it costs a proof-of-work solution.
            assert_equal(cr["sigma_seed"], bh)
            assert_equal(cr["payout_height"], h + 1)
            # Both nodes must resolve the same draw from the same block.
            assert_equal(node_b.getconvergenceresult(bh), cr)

        self.log.info("Heights 1..10 carry no bootstrap special case and seed on themselves: PASS")


if __name__ == "__main__":
    BootstrapDeterministicBlocksTest().main()
