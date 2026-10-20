#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Regression test for MINER-BUG-01.

Verifies that getblocktemplate exposes the new ``prev_nbits`` field which
miners MUST use to populate the coinbase RIGOR commitment payload tail.

Pre-fix failure mode
--------------------
Miners derived RIGOR ``res_target_nbits`` from the template ``bits`` (next-block
target). Consensus (``validation.cpp::CheckRigorDrawTargetCommitment``) requires
``res_target_nbits == pindex->pprev->nBits``. At difficulty-retarget boundaries
``next_bits != prev_bits`` and miner-produced blocks were rejected with
``bad-rigor-target-mismatch``.

Post-fix invariants asserted here
---------------------------------
1. GBT response carries ``prev_nbits`` as an 8-char hex string.
2. ``prev_nbits`` equals the compact ``bits`` of the current chain tip
   (i.e. ``pindexPrev->nBits`` from the next block's perspective).
3. The relationship holds across multiple heights including immediately after
   block generation (template re-derivation path).
4. ``prev_nbits`` and template ``bits`` are independent fields (additive-only
   GBT schema change; ``bits`` semantics unchanged).
"""

from test_framework.blocktools import NORMAL_GBT_REQUEST_PARAMS
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class MiningPrevNbitsTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.supports_cli = False

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]

        # Generate a small chain so getblocktemplate has a non-genesis tip.
        addr = node.getnewaddress()
        self.generatetoaddress(node, 5, addr)

        for height_check in range(3):
            tip_hash = node.getbestblockhash()
            tip_header = node.getblockheader(tip_hash)
            expected_prev_nbits = tip_header['bits']  # already lowercase 8-hex

            tmpl = node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)

            # Invariant 1: field present and well-formed.
            assert 'prev_nbits' in tmpl, \
                "getblocktemplate MUST expose prev_nbits (contract)"
            assert isinstance(tmpl['prev_nbits'], str), \
                "prev_nbits MUST be a hex string"
            assert_equal(len(tmpl['prev_nbits']), 8)
            int(tmpl['prev_nbits'], 16)  # parses as hex

            # Invariant 2: equals tip's bits.
            assert_equal(tmpl['prev_nbits'], expected_prev_nbits)

            # Invariant 4: prev_nbits and bits are independent fields; both present.
            assert 'bits' in tmpl
            assert isinstance(tmpl['bits'], str)
            assert_equal(len(tmpl['bits']), 8)

            # Advance one block and re-check (Invariant 3).
            self.generatetoaddress(node, 1, addr)

        self.log.info("MINER-BUG-01 closure verified: GBT exposes prev_nbits == pindexPrev->nBits")


if __name__ == '__main__':
    MiningPrevNbitsTest().main()
